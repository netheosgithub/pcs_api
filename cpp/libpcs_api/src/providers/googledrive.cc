/**
 * Copyright (c) 2014 Netheos (http://www.netheos.net)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vector>
#include <string>
#include <memory>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/replace.hpp"
#include "boost/format.hpp"
#include "boost/date_time/posix_time/posix_time.hpp"

#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/uri_builder.h"
#include "cpprest/http_msg.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/interopstream.h"

#include "pcs_api/c_exceptions.h"
#include "pcs_api/memory_byte_source.h"
#include "pcs_api/internal/providers/googledrive.h"
#include "pcs_api/internal/retry_401_once_response_validator.h"
#include "pcs_api/internal/c_folder_content_builder.h"
#include "pcs_api/internal/storage_provider.h"
#include "pcs_api/internal/json_utils.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/form_body_builder.h"
#include "pcs_api/internal/multipart_streambuf.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

const char* GoogleDrive::kProviderName = "googledrive";

static const char_t *kEndPoint = U("https://www.googleapis.com/drive/v2");
static const char_t *kFilesEndPoint =
                                U("https://www.googleapis.com/drive/v2/files");
static const char_t *kFilesUploadEndPoint =
                        U("https://www.googleapis.com/upload/drive/v2/files");
static const char_t *kUserInfoEndPoint =
                            U("https://www.googleapis.com/oauth2/v1/userinfo");
static const char_t *kOAuthRoot =
                                    U("https://accounts.google.com/o/oauth2");
static const char_t *kMimeTypeDirectory =
                                    U("application/vnd.google-apps.folder");

StorageBuilder::create_provider_func GoogleDrive::GetCreateInstanceFunction() {
    return GoogleDrive::CreateInstance;
}

std::shared_ptr<IStorageProvider> GoogleDrive::CreateInstance(
                                            const StorageBuilder& builder) {
    // make_shared not usable because constructor is private
    return std::shared_ptr<IStorageProvider>(new GoogleDrive(builder));
}


GoogleDrive::GoogleDrive(const StorageBuilder& builder)
    : StorageProvider(
        builder.provider_name(),
        std::make_shared<OAuth2SessionManager>(
                string_t(kOAuthRoot)
                    + U("/auth?access_type=offline&approval_prompt=force"),
                string_t(kOAuthRoot) + U("/token"),  // access_token_url,
                string_t(kOAuthRoot) + U("/token"),  // refresh_token_url,
                true,  // scope_in_authorization,
                ' ',  // scope_perms_separator
                builder),
        builder.retry_strategy()) {
}

void GoogleDrive::ThrowCStorageException(CResponse *p_response,
                                         const CPath* p_opt_path) {
    // Try to extract error message from json body:
    std::string message;
    if (p_response->IsJsonContentType()) {
        std::string json_str;
        try {
            web::json::value json = p_response->AsJson();
            json_str = utility::conversions::to_utf8string(json.serialize());
            web::json::value error = json.at(U("error"));
            int jcode = error.at(U("code")).as_integer();
            std::string jreason = utility::conversions::to_utf8string(
                    error.at(U("errors")).at(0).at(U("reason")).as_string());
            std::ostringstream tmp;
            // do not change, used below:
            tmp << boost::format("[%i/%s] ") % jcode % jreason;
            message = tmp.str();
            tmp << utility::conversions::to_utf8string(
                    error.at(U("message")).as_string());
            message = tmp.str();
            if (jcode == 403 && jreason == "userAccess" && p_opt_path) {
                // permission error: indicate failing path helps:
                tmp << " (" << p_opt_path->path_name_utf8() << ")";
                message = tmp.str();
            }
        }
        catch (web::json::json_exception&) {
            LOG_WARN << "Unparsable server error message: " << json_str;
            // we failed... server returned strange string
        }
    }
    p_response->ThrowCStorageException(message, p_opt_path);
}

/**
 * \brief Validate a response from google drive for files downloads.
 *
 * Only server code is checked (content-type is ignored).
 * Request is retriable in case of server error 5xx or some 403 errors related
 * to rate limit.
 */
void GoogleDrive::ValidateGoogleDriveResponse(CResponse *p_response,
                                              const CPath* p_opt_path) {
    LOG_DEBUG << "Validating GoogleDrive response: " << p_response->ToString();
    bool retriable = false;
    if (p_response->status() >= 500) {
        retriable = true;
    }
    // see below for [403/rateLimitExceeded]: this request is retriable
    if (p_response->status() >= 300) {
        try {
            // We have to throw in order to parse payload
            ThrowCStorageException(p_response, p_opt_path);
        }
        catch (const std::exception& e) {
            std::string msg = e.what();
            if (   boost::algorithm::starts_with(msg,
                                            "[403/rateLimitExceeded]")
                || boost::algorithm::starts_with(msg,
                                            "[403/userRateLimitExceeded]")) {
                retriable = true;
            }
            if (!retriable) {
                throw;  // rethrow directly
            }
            BOOST_THROW_EXCEPTION(
                                CRetriableException(std::current_exception()));
        }
    }
    // OK, response looks fine
}

/**
 * \brief Validate a response from google drive API.
 *
 * content-type must be json.
 */
void GoogleDrive::ValidateGoogleDriveApiResponse(CResponse *p_response,
                                                 const CPath* p_opt_path) {
    ValidateGoogleDriveResponse(p_response, p_opt_path);
    // In addition, check content type is json:
    p_response->EnsureContentTypeIsJson(true);
    // OK, API response looks fine
}

/**
 * \brief An invoker that checks response content type = json: to be used by
 *        all API requests.
 *
 * @param p_opt_path (optional) context for the request: path of remote file
 * @return a request invoker specific for API requests
 */
RequestInvoker GoogleDrive::GetApiRequestInvoker(const CPath* p_opt_path) {
    // Request execution is delegated to our session manager, response
    // validation is done thanks to a Retry401OnceResponseValidator object,
    // ref counted so that it is deleted once no more bound
    // (hence when RequestInvoker is destroyed):
    std::shared_ptr<Retry401OnceResponseValidator> p_validator_object =
        std::make_shared<Retry401OnceResponseValidator>(
            p_session_manager_.get(),
            std::bind(&GoogleDrive::ValidateGoogleDriveApiResponse,
                      this,
                      std::placeholders::_1,
                      std::placeholders::_2));  // validate_func
    return RequestInvoker(
        std::bind(&OAuth2SessionManager::Execute,
                p_session_manager_.get(),
                std::placeholders::_1),  // do_request_func
        std::bind(&Retry401OnceResponseValidator::ValidateResponse,
                p_validator_object,
                std::placeholders::_1,
                std::placeholders::_2),  // validate_func
        p_opt_path);
}

/**
* \brief An invoker that does not check response content type:
*        to be used for files downloads.
*
* @param path context for the request: path of remote file
* @return a request invoker specific for file download requests
*/
RequestInvoker GoogleDrive::GetRequestInvoker(const CPath* p_path) {
    // Request execution is delegated to our session manager,
    // response validation is done here:
    return RequestInvoker(
        std::bind(&OAuth2SessionManager::Execute,
                  p_session_manager_.get(),
                  std::placeholders::_1),  // do_request_func
        // validate_func:
        std::bind(&GoogleDrive::ValidateGoogleDriveResponse,
                  this,
                  std::placeholders::_1, std::placeholders::_2),
        p_path);
}

static string_t GetFileUrl(string_t file_id) {
    return string_t(kEndPoint) + U("/files/") + file_id;
}


GoogleDrive::RemotePath::RemotePath(const CPath& path,
                                    std::vector<web::json::value> files_chain)
    : path_(path),
      segments_(std::move(path.Split())),
      files_chain_(files_chain) {
}

bool GoogleDrive::RemotePath::Exists() const {
    return files_chain_.size() == segments_.size();
}

string_t GoogleDrive::RemotePath::GetDeepestFolderId() const {
    if (files_chain_.empty()) {
        return U("root");
    }
    web::json::value last = *files_chain_.crbegin();
    if (string_t(kMimeTypeDirectory) == last.at(U("mimeType")).as_string()) {
        return last.at(U("id")).as_string();
    }
    // If last is a blob, we return the parent id:
    if (files_chain_.size() == 1) {
        return U("root");
    }
    return files_chain_[files_chain_.size() - 2].at(U("id")).as_string();
}

const web::json::value& GoogleDrive::RemotePath::GetBlob() const {
    if (!LastIsBlob()) {
        BOOST_THROW_EXCEPTION(
            std::logic_error("Inquiring blob of a folder for "
                             + path_.path_name_utf8()));
    }
    return *files_chain_.rbegin();
}

bool GoogleDrive::RemotePath::LastIsBlob() const {
    return !files_chain_.empty()
        && string_t(kMimeTypeDirectory) !=
           files_chain_[files_chain_.size()-1].at(U("mimeType")).as_string();
}

CPath GoogleDrive::RemotePath::GetFirstSegmentsPath(size_t depth) {
    std::basic_ostringstream<char_t> pathname;
    pathname << U("/");
    for (unsigned int i = 0; i < depth; i++) {
        if (i >= segments_.size()) {
            break;
        }
        pathname << segments_[i] << U("/");
    }
    return CPath(pathname.str());
}

CPath GoogleDrive::RemotePath::LastCPath() {
    return GetFirstSegmentsPath(files_chain_.size());
}


/**
 * \brief Resolve the given CPath to gather informations (mainly id and
 *        mimeType) ; returns a RemotePath object.
 *
 * Drive API does not allow this natively ; we perform a single request that
 * returns all files (but may return too much): find files with title='a' or
 * title='b' or title='c', then we connect children and parents to get the
 * chain of ids. TODO This fails if there are several folders with same name,
 * and we follow the wrong "branch".
 */
const GoogleDrive::RemotePath GoogleDrive::FindRemotePath(const CPath& path,
                                                          bool detailed) {
    // easy special case:
    if (path.IsRoot()) {
        return RemotePath(path, std::vector<web::json::value>());
    }
    // Here we know that we have at least one path segment

    // Build query (https://developers.google.com/drive/web/search-parameters)
    std::vector<string_t> segments = path.Split();
    std::basic_ostringstream<char_t> query;
    query << U("(");
    int i = 0;
    for (const string_t& segment : segments) {
        if (i > 0) {
            query << U(" or ");
        }
        // escape ' in segments --> \'
        query << U("(title='")
              << boost::algorithm::replace_all_copy(segment, U("'"), U("\\'"))
              << U("'");
        query << U(")");
        i++;
    }
    query << U(") and trashed=false");

    // drive may not return all results in a single query:
    // ouch there seems to be some issues with pagination on the google side ?
    // http://stackoverflow.com/questions/18646004/drive-api-files-list-query-with-not-parameter-returns-empty-pages?rq=1
    // http://stackoverflow.com/questions/18355113/paging-in-files-list-returns-endless-number-of-empty-pages?rq=1
    // http://stackoverflow.com/questions/19679190/is-paging-broken-in-drive?rq=1
    // http://stackoverflow.com/questions/16186264/files-list-reproducibly-returns-incomplete-list-in-drive-files-scope
    std::vector<web::json::value> items;
    string_t next_page_token;
    while (true) {
        // Execute request ; we ask for specific fields only
        string_t fields_filter =
                            U("id,title,mimeType,parents/id,parents/isRoot");
        if (detailed) {
            fields_filter += U(",downloadUrl,modifiedDate,fileSize");
        }
        fields_filter = string_t(U("nextPageToken,items("))
                        + fields_filter + U(")");
        web::uri uri(kFilesEndPoint);
        web::uri_builder builder(uri);
        builder.append_query(U("q="
                                + web::uri::encode_data_string(query.str())));
        builder.append_query(U("fields"), fields_filter);
        if (!next_page_token.empty()) {
            builder.append_query(U("pageToken"), next_page_token);
        }
        builder.append_query(U("maxResults"), 1000);

        RequestInvoker ri = GetApiRequestInvoker();
        std::shared_ptr<CResponse> p_response;
        p_retry_strategy_->InvokeRetry([&] {
            web::http::http_request request(web::http::methods::GET);
            request.set_request_uri(builder.to_uri());
            p_response = ri.Invoke(request);
        });
        web::json::value jresp = std::move(p_response->AsJson());
        web::json::array& items_in_page = jresp.at(U("items")).as_array();
        for (web::json::value item : items_in_page) {
            items.push_back(item);
        }
        // Is it the last page ?
        next_page_token = JsonForKey(jresp, U("nextPageToken"), string_t());
        if (!next_page_token.empty()) {
            LOG_TRACE << "FindRemotePath() will loop: ("
                      << items_in_page.size() << " items in this page)";
        } else {
            // LOG_TRACE << "FindRemotePath(): no more data for this query";
            break;
        }
    }

    // Now connect parent/children to build the path:
    std::vector<web::json::value> files_chain;
    // this changes parent condition (isRoot, or no parent for shares):
    bool first_segment = true;
    for (const string_t& searched_segment : segments) {
        // print("searching segment ",searched_segment)
        web::json::value next_item = web::json::value::null();
        for (const web::json::value& item : items) {
            // We match title
            if (item.at(U("title")).as_string() == searched_segment) {
                const web::json::value& parents = item.has_field(U("parents")) ?
                                                    item.at(U("parents")) :
                                                    web::json::value::null();
                if (first_segment) {
                    if (parents.is_null() || parents.as_array().size() == 0) {
                        // no parents (shared folder ?)
                        next_item = item;
                        break;
                    }
                    for (unsigned int k = 0;
                         k < parents.as_array().size();
                         k++) {
                        const web::json::value& p = parents.at(k);
                        if (JsonForKey(p, U("isRoot"), false)) {
                            // at least one parent is root
                            next_item = item;
                            break;
                        }
                    }
                } else {
                    string_t last_parent_id = files_chain.back().at(U("id"))
                                                    .as_string();
                    for (unsigned int k = 0;
                         k < parents.as_array().size();
                         k++) {
                        const web::json::value& p = parents.at(k);
                        if (p.at(U("id")).as_string() == last_parent_id) {
                            // at least one parent id is equals
                            // to last parent id: connect
                            next_item = item;
                            break;
                        }
                    }
                }
                if (!next_item.is_null()) {
                    break;
                }
            }
        }
        if (next_item.is_null()) {
            break;
        }
        files_chain.push_back(next_item);
        first_segment = false;
    }
    return GoogleDrive::RemotePath(path, files_chain);
}


std::string GoogleDrive::GetUserId() {
    // user_id is email in case of googledrive
    string_t url = kUserInfoEndPoint;
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(url));
        p_response = ri.Invoke(request);
    });
    web::json::value json = p_response->AsJson();
    return utility::conversions::to_utf8string(json.at(U("email")).as_string());
}

CQuota GoogleDrive::GetQuota() {
    // Return a CQuota object
    string_t url = string_t(kEndPoint) + U("/about");
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(url));
        p_response = ri.Invoke(request);
    });
    web::json::value json = p_response->AsJson();
    // doc says it is a long value, in practice these are strings
    // so we try to be conservative: JSonForKey() handles both types
    return CQuota(JsonForKey(json, U("quotaBytesUsed"), (int64_t)-1),
                  JsonForKey(json, U("quotaBytesTotal"), (int64_t)-1));
}

std::shared_ptr<CFolderContent> GoogleDrive::ListRootFolder() {
    return ListFolder(CPath(U("/")));
}

std::shared_ptr<CFolderContent> GoogleDrive::ListFolder(const CPath& path) {
    std::shared_ptr<CFolderContent> p_ret;
    RemotePath remote_path = FindRemotePath(path, true);
    if (!remote_path.Exists()) {
        // per contract, listing a non existing folder
        // must return an empty pointer
        return p_ret;  // empty
    }
    if (remote_path.LastIsBlob()) {
        BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, false));
    }
    // Now we inquire for children of leaf folder:
    string_t folder_id = remote_path.GetDeepestFolderId();
    std::basic_ostringstream<char_t> query;
    query << U("('") << folder_id << U("' in parents");
    if (path.IsRoot()) {
        // If we list root folder, also list shared files, as they appear here:
        query << U(" or sharedWithMe");
    }
    query << U(") and trashed=false");
    string_t fields_filter =
        U("nextPageToken,items(id,title,mimeType,fileSize,modifiedDate)");
    web::uri_builder builder = web::uri_builder(web::uri(kFilesEndPoint));
    builder.append_query(U("q"), query.str());
    builder.append_query(U("fields"), fields_filter);
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(builder.to_uri());
        p_response = ri.Invoke(request);
    });
    web::json::value json = std::move(p_response->AsJson());

    CFolderContentBuilder cfcb;
    const web::json::value& array = json.at(U("items"));
    for (unsigned int i = 0; i < array.size(); i++) {
        const web::json::value &item_obj = array.at(i);
        std::shared_ptr<CFile> p_file = ParseCFile(path, item_obj);
        cfcb.Add(p_file->path(), p_file);
    }
    return cfcb.BuildFolderContent();
}

std::shared_ptr<CFolderContent> GoogleDrive::ListFolder(const CFolder& folder) {
    return ListFolder(folder.path());
}

/**
 * \brief Create a folder without creating any higher level intermediate
 *        folders, and return id of created folder.
 *
 * @param path
 * @param parent_id
 * @return id of created folder
 */
string_t GoogleDrive::RawCreateFolder(const CPath& path, string_t parent_id) {
    RequestInvoker ri = GetApiRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::json::value body;
        body[U("title")] = web::json::value::string(path.GetBaseName());
        body[U("mimeType")] = web::json::value::string(kMimeTypeDirectory);
        web::json::value ids;
        web::json::value id_obj;
        id_obj[U("id")] = web::json::value::string(parent_id);
        ids[0] = id_obj;
        body[U("parents")] = ids;

        web::http::http_request request(web::http::methods::POST);
        request.set_request_uri(string_t(kFilesEndPoint) + U("?fields=id"));
        request.set_body(body);

        p_response = ri.Invoke(request);
    });
    web::json::value jresp = p_response->AsJson();
    return jresp.at(U("id")).as_string();
}


bool GoogleDrive::CreateFolder(const CPath& path) {
    // we have to check before if folder already exists:
    // (and also to determine what folders must be created)
    RemotePath remote_path = FindRemotePath(path, false);
    if (remote_path.LastIsBlob()) {
        // A blob exists along that path: wrong !
        BOOST_THROW_EXCEPTION(
                CInvalidFileTypeException(remote_path.LastCPath(), false));
    }
    if (remote_path.Exists()) {
        // folder already exists:
        return false;
    }

    // we may have to create any intermediate folders:
    string_t parent_id = remote_path.GetDeepestFolderId();
    size_t i = remote_path.files_chain().size();
    while (i < remote_path.segments().size()) {
        CPath current_path = remote_path.GetFirstSegmentsPath(i + 1);
        parent_id = RawCreateFolder(current_path, parent_id);
        i++;
    }
    return true;
}

void GoogleDrive::DeleteById(const CPath& path, string_t file_id) {
    string_t url = GetFileUrl(file_id) + U("/trash");

    RequestInvoker ri = GetApiRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::POST);
        request.set_request_uri(web::uri(url));
        request.headers().set_content_length(0);
        p_response = ri.Invoke(request);
    });
}

bool GoogleDrive::Delete(const CPath& path) {
    // Move file to trash
    if (path.IsRoot()) {
        BOOST_THROW_EXCEPTION(CStorageException("Can not delete root folder"));
    }

    RemotePath remote_path = FindRemotePath(path, false);
    if (!remote_path.Exists()) {
        return false;
    }
    // We have at least one segment ; this is either a folder or a blob
    // (so we cannot rely on DeepestFolderId() as it works only for folders)
    DeleteById(path, remote_path.files_chain().back().at(U("id")).as_string());
    return true;
}

std::shared_ptr<CFile> GoogleDrive::GetFile(const CPath& path) {
    std::shared_ptr<CFile> p_ret;
    if (path.IsRoot()) {
        // no last modified date for root folder:
        p_ret.reset(new CFolder(CPath(U("/")), boost::posix_time::ptime()));
        return p_ret;
    }
    RemotePath remote_path = FindRemotePath(path, true);
    if (!remote_path.Exists()) {
        return p_ret;  // empty
    }
    return ParseCFile(path.GetParent(), *remote_path.files_chain().crbegin());
}

void GoogleDrive::Download(const CDownloadRequest& download_request) {
    const CPath& path = download_request.path();
    RequestInvoker ri = GetRequestInvoker(&path);
    p_retry_strategy_->InvokeRetry([&] {
        RemotePath remote_path = FindRemotePath(path, true);
        if (!remote_path.Exists()) {
            BOOST_THROW_EXCEPTION(
                CFileNotFoundException("File not found: "
                                           + path.path_name_utf8(),
                                       path));
        }
        if (!remote_path.LastIsBlob()) {
            // path refer to an existing folder: wrong !
            BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
        }

        web::json::value blob = remote_path.GetBlob();
        if (!blob.has_field(U("downloadUrl"))) {
            // A blob without a download url is likely a google doc,
            // these are not downloadable:
            if (blob.has_field(U("mimeType"))
                && boost::algorithm::starts_with(
                        blob.at(U("mimeType")).as_string(),
                        U("application/vnd.google-apps."))) {
                LOG_ERROR << "google docs are not downloadable: "
                          << path.path_name_utf8();
                BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
            }
            BOOST_THROW_EXCEPTION(
                CStorageException("No downloadUrl defined for blob: "
                                    + path.path_name_utf8()));
        }
        string_t url = blob.at(U("downloadUrl")).as_string();
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(url));
        for (std::pair<string_t, string_t> header :
                                        download_request.GetHttpHeaders()) {
            request.headers().add(header.first, header.second);
        }
        std::shared_ptr<CResponse> p_response = ri.Invoke(request);
        std::shared_ptr<ByteSink> p_byte_sink = download_request.GetByteSink();
        p_response->DownloadDataToSink(p_byte_sink.get());
    });
}

void GoogleDrive::Upload(const CUploadRequest& upload_request) {
    p_retry_strategy_->InvokeRetry([&]{
        // Check before upload: is it a folder ?
        // (uploading a blob would create another file with the same name: bad)
        const CPath& path = upload_request.path();
        RemotePath remote_path = FindRemotePath(path, false);
        if (remote_path.Exists() && !remote_path.LastIsBlob()) {
            // path refer to an existing folder: wrong !
            BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
        }
        if (!remote_path.Exists() && remote_path.LastIsBlob()) {
            // some blob exists in path: wrong !
            BOOST_THROW_EXCEPTION(
                CInvalidFileTypeException(remote_path.LastCPath(), false));
        }

        // only one of these 2 will be set:
        string_t file_id;
        string_t parent_id;
        if (remote_path.Exists()) {
            // Blob already exists: we'll update it
            file_id = remote_path.GetBlob().at(U("id")).as_string();
        } else {
            parent_id = remote_path.GetDeepestFolderId();
            // We may need to create intermediate folders first:
            size_t i = remote_path.files_chain().size();
            while (i < remote_path.segments().size() - 1) {
                const CPath current_path =
                                    remote_path.GetFirstSegmentsPath(i + 1);
                parent_id = RawCreateFolder(current_path, parent_id);
                i++;
            }
        }

        // By now we can upload a new blob to folder with id=parent_id,
        // or update existing blob with id=file_id:
        web::json::value json_meta = web::json::value::object();
        if (!file_id.empty()) {
            // Blob update
        } else {
            // Blob creation
            json_meta[U("title")] =
                                web::json::value::string(path.GetBaseName());
            web::json::value ids_array = web::json::value::array();
            web::json::value id_obj = web::json::value::object();
            id_obj[U("id")] = web::json::value::string(parent_id);
            ids_array[0] = id_obj;
            json_meta[U("parents")] = ids_array;
        }
        if (!upload_request.content_type().empty()) {
            // It seems that drive distinguishes between mimeType defined here,
            // and Content-Type defined in part header.
            // Drive also tries to guess mimeType...
            json_meta[U("mimeType")] =
                web::json::value::string(upload_request.content_type());
        }

        MultipartStreamer mp_streamer("related");
        // metadata part:
        MemoryByteSource metadata(
                utility::conversions::to_utf8string(json_meta.serialize()));
        MultipartStreamer::Part metadata_part("", &metadata);
        metadata_part.AddHeader("Content-Type",
                                "application/json; charset=UTF-8");
        mp_streamer.AddPart(metadata_part);
        // media part:
        std::shared_ptr<ByteSource> p_media_source =
                                                upload_request.GetByteSource();
        MultipartStreamer::Part media_part("", p_media_source.get());
        media_part.AddHeader(
            "Content-Type",
            utility::conversions::to_utf8string(upload_request.content_type()));
        mp_streamer.AddPart(media_part);

        // Prepare stream and adapt to cpprest:
        MultipartStreambuf mp_streambuf(&mp_streamer);
        std::istream mp_istream(&mp_streambuf);
        // Adapt std::istream to asynchronous concurrency istream:
        concurrency::streams::stdio_istream<uint8_t> is_wrapper(mp_istream);

        web::http::http_request request;
        if (!file_id.empty()) {
            // Updating existing file:
            request.set_method(web::http::methods::PUT);
            request.set_request_uri(string_t(kFilesUploadEndPoint)
                                    + U("/")
                                    + file_id + U("?uploadType=multipart"));
        } else {
            // uploading a new file:
            request.set_method(web::http::methods::POST);
            request.set_request_uri(string_t(kFilesUploadEndPoint)
                                    + U("?uploadType=multipart"));
        }
        request.set_body(is_wrapper,
                         mp_streamer.ContentLength(),
                         mp_streamer.ContentType());

        RequestInvoker ri = GetApiRequestInvoker(&path);
        std::shared_ptr<CResponse> p_response = ri.Invoke(request);
        // We are not interested in response
    });
}

std::shared_ptr<CFile> GoogleDrive::ParseCFile(const CPath& parent_path,
                                               const web::json::value& json) {
    std::string date_str = JsonForKey(json, U("modifiedDate"), std::string());
    try {
        std::shared_ptr<CFile> p_file;
        std::locale loc(std::locale::classic(),
                new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%S%F"));
        std::istringstream is(date_str);
        is.imbue(loc);
        ::boost::posix_time::ptime modified;
        is >> modified;
        // LOG_DEBUG << "date_str = " << date_str
        //           << "   parsed ptime = " << modified;
        string_t title = JsonForKey(json, U("title"), string_t());
        CPath file_path = parent_path.Add(title);
        string_t mime_type = JsonForKey(json, U("mimeType"), string_t());
        if (string_t(kMimeTypeDirectory) == mime_type) {
            p_file = std::make_shared<CFolder>(file_path,
                                               modified);
        } else {
            // google apps files (application/vnd.google-apps.document,
            // application/vnd.google-apps.spreadsheet, etc.)
            // do not publish any size (they can not be downloaded, only
            // exported). In this case file_size=-1
            int64_t file_size = JsonForKey(json, U("fileSize"), (int64_t)-1);
            p_file = std::make_shared<CBlob>(file_path,
                                             file_size,
                                             mime_type,
                                             modified);
        }
        return p_file;
    }
    catch (web::json::json_exception&) {
        BOOST_THROW_EXCEPTION(CStorageException("Can't parse file json",
                                                std::current_exception()));
    }
}


}  // namespace pcs_api
