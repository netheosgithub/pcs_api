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

#include "boost/date_time/posix_time/posix_time_io.hpp"

#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/uri_builder.h"
#include "cpprest/http_msg.h"
#include "cpprest/http_helpers.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/interopstream.h"

#include "pcs_api/internal/providers/dropbox.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/c_folder_content_builder.h"
#include "pcs_api/internal/storage_provider.h"
#include "pcs_api/internal/json_utils.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/form_body_builder.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

const char* Dropbox::kProviderName = "dropbox";

static const char_t * kEndPoint = U("https://api.dropbox.com/1");
static const char_t * kContentEndPoint = U("https://api-content.dropbox.com/1");
static const char_t * kMetadata = U("metadata");

StorageBuilder::create_provider_func Dropbox::GetCreateInstanceFunction() {
    return Dropbox::CreateInstance;
}

std::shared_ptr<IStorageProvider> Dropbox::CreateInstance(
                                            const StorageBuilder& builder) {
    // make_shared not usable because constructor is private
    return std::shared_ptr<IStorageProvider>(new Dropbox(builder));
}


Dropbox::Dropbox(const StorageBuilder& builder) :
    StorageProvider(builder.provider_name(),
                    std::make_shared<OAuth2SessionManager>(
                            // authorize_url:
                            string_t(kEndPoint) + U("/oauth2/authorize"),
                            // access_token_url:
                            string_t(kEndPoint) + U("/oauth2/token"),
                            U(""),  // refresh_token_url, not used
                            false,  // scope_in_authorization,
                            ' ',  // scope_perms_separator (not used))
                            builder),
                    builder.retry_strategy()) {
    std::vector<std::string> perms = p_session_manager_->app_info().scope();
    if (perms.empty()) {
        BOOST_THROW_EXCEPTION(
                    CStorageException("Missing scope for Dropbox provider"));
    }
    scope_ = utility::conversions::to_string_t(perms[0]);
    if (scope_ != U("dropbox") && scope_ != U("sandbox")) {
        BOOST_THROW_EXCEPTION(
            CStorageException("Invalid scope for Dropbox provider:"
                              " expected 'dropbox' or 'sandbox'"));
    }
}

void Dropbox::ThrowCStorageException(CResponse *p_response,
                                     std::string msg,
                                     const CPath* p_opt_path,
                                     bool throw_retriable) {
    if (p_response->IsJsonContentType()) {
        web::json::value json = p_response->AsJson();
        if (json.is_object()) {
            string_t dropbox_msg = JsonForKey(json,
                                              U("error"),
                                              string_t(U("")));
            if (!dropbox_msg.empty()) {
                msg = utility::conversions::to_utf8string(dropbox_msg);
            }
        }
    }
    if (!throw_retriable) {
        p_response->ThrowCStorageException(msg, p_opt_path);
    }
    try {
        p_response->ThrowCStorageException(msg, p_opt_path);
    }
    catch (const std::exception&) {
        BOOST_THROW_EXCEPTION(CRetriableException(std::current_exception()));
    }
}

/**
 * \brief Validate a response from dropbox API.
 *
 * A response is valid if server code is 2xx and content-type JSON.
 * Request is retriable in case of server error 5xx (except 507)."""
 *
 */
void Dropbox::ValidateDropboxApiResponse(CResponse *p_response,
                                         const CPath* p_opt_path) {
    ValidateDropboxResponse(p_response, p_opt_path);
    // Server response looks correct ; however check content type is json:
    p_response->EnsureContentTypeIsJson(true);
    // OK, response looks fine
}

/**
 * \brief Validate a response for a file download or API request.
 *
 * Only server code is checked (content-type is ignored).
 * Request is retriable in case of server error 5xx (except 507)."""
 *
 */
void Dropbox::ValidateDropboxResponse(CResponse *p_response,
                                      const CPath* p_opt_path) {
    LOG_DEBUG << "Validating dropbox response: " << p_response->ToString();

    if (p_response->status() == 507) {
        // User is over Dropbox storage quota: no need to retry then
        ThrowCStorageException(p_response, "Quota exceeded", p_opt_path);
    }
    if (p_response->status() >= 500) {
        ThrowCStorageException(p_response, "", p_opt_path, true);  // retriable
    }
    if (p_response->status() >= 300) {
        ThrowCStorageException(p_response, "", p_opt_path);
    }
    // OK, response looks fine
}


/**
 * \brief An invoker that checks response content type = json:
 * to be used by all API requests.
 *
 * @param p_opt_path (optional) context for the request: path of remote file
 * @return a request invoker specific for API requests
 */
RequestInvoker Dropbox::GetApiRequestInvoker(const CPath* p_opt_path) {
    // Request execution is delegated to our session manager,
    // response validation is done here:
    return RequestInvoker(std::bind(&OAuth2SessionManager::Execute,
                                    p_session_manager_.get(),
                                    std::placeholders::_1),  // do_request_func
                          std::bind(&Dropbox::ValidateDropboxApiResponse,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2),  // validate_func
                          p_opt_path);
}

/**
 * \brief An invoker that does not check response content type:
 * to be used for files downloads.
 *
 * @param path context for the request: path of remote file
 * @return a request invoker specific for file download requests
 */
RequestInvoker Dropbox::GetRequestInvoker(const CPath* p_path) {
    // Request execution is delegated to our session manager,
    // response validation is done here:
    return RequestInvoker(std::bind(&OAuth2SessionManager::Execute,
                                    p_session_manager_.get(),
                                    std::placeholders::_1),  // do_request_func
                          std::bind(&Dropbox::ValidateDropboxResponse,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2),  // validate_func
                          p_path);
}

const web::json::value Dropbox::GetAccount() {
    web::uri_builder builder(kEndPoint);
    builder.append_path(U("/account/info"));

    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(builder.to_uri());
        p_response = ri.Invoke(request);
    });

    return p_response->AsJson();
}

std::string Dropbox::GetUserId() {
    const web::json::value json_account = GetAccount();
    return utility::conversions::to_utf8string(
                                    json_account.at(U("email")).as_string());
}

CQuota Dropbox::GetQuota() {
    const web::json::value json_quota = GetAccount().at(U("quota_info"));
    int64_t shared = json_quota.at(U("shared")).as_number().to_int64();
    int64_t normal = json_quota.at(U("normal")).as_number().to_int64();
    int64_t total = json_quota.at(U("quota")).as_number().to_int64();
    return CQuota(shared + normal, total);
}

string_t Dropbox::BuildUrl(const string_t& root, const string_t& method_path) {
    string_t url(root);
    url += U('/');
    url += method_path;
    return url;
}

void Dropbox::AddPathToUrl(string_t* p_url, const CPath& path) {
    *p_url += U('/');
    *p_url += scope_;
    *p_url += path.GetUrlEncoded();
}

string_t Dropbox::BuildApiUrl(const string_t& method_path) {
    return BuildUrl(kEndPoint, method_path);
}

/**
 * Url encodes object path, and concatenate to API endpoint
 * and method to get full URL.
 *
 * @param methodPath API method path
 * @param path path
 * @return built url
 */
string_t Dropbox::BuildFileUrl(const string_t& method_path, const CPath& path) {
    string_t url = BuildApiUrl(method_path);
    AddPathToUrl(&url, path);
    return url;
}

string_t Dropbox::BuildContentUrl(string_t method_path, CPath path) {
    string_t url = BuildUrl(kContentEndPoint, method_path);
    AddPathToUrl(&url, path);
    return url;
}

std::shared_ptr<CFolderContent> Dropbox::ListRootFolder() {
    return ListFolder(CPath(U("/")));
}

std::shared_ptr<CFolderContent> Dropbox::ListFolder(const CPath& path) {
    string_t url = BuildFileUrl(kMetadata, path);

    RequestInvoker ri = GetApiRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    try {
        p_retry_strategy_->InvokeRetry([&] {
            web::http::http_request request(web::http::methods::GET);
            request.set_request_uri(url);
            p_response = ri.Invoke(request);
        });
    } catch (CFileNotFoundException&) {
        // Non existing folder
        return std::shared_ptr<CFolderContent>();
    }

    web::json::value jvalue = p_response->AsJson();
    web::json::object& j_object = jvalue.as_object();
    auto it = j_object.find(U("is_deleted"));
    if (it != j_object.end() && it->second.as_bool()) {
        // File is logically deleted
        return std::shared_ptr<CFolderContent>();
    }
    it = j_object.find(U("is_dir"));
    if (it == j_object.end()) {
        p_response->ThrowCStorageException("No 'is_dir' key in JSON metadata",
                                           &path);
    }
    if (!it->second.as_bool()) {
        BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, false));
    }

    CFolderContentBuilder cfcb;
    const web::json::array content = j_object.at(U("contents")).as_array();
    for (auto ait = content.cbegin() ; ait != content.cend() ; ++ait) {
        std::shared_ptr<CFile> p_cfile = ParseCFile(ait->as_object());
        cfcb.Add(p_cfile->path(), p_cfile);
    }
    return cfcb.BuildFolderContent();
}

static boost::posix_time::ptime ParseDateTime(const std::string& date_string) {
    // as per Dropbox API doc, timezone is always UTC
    // (string_date always ends with +0000)
    // Thus we can parse without timezone:
    std::locale temploc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%a, %d %b %Y %H:%M:%S %ZP"));
    std::istringstream tempis(date_string);
    tempis.imbue(temploc);
    boost::posix_time::ptime posix_time;
    tempis >> posix_time;
    // LOG_DEBUG << "date_string=" << date_string << "  ptime=" << posix_time;
    return posix_time;
}

std::shared_ptr<CFile> Dropbox::ParseCFile(const web::json::object& file_obj) {
    std::shared_ptr<CFile> p_cfile;

    CPath path = CPath(file_obj.at(U("path")).as_string());
    string_t date_string = file_obj.at(U("modified")).as_string();
    boost::posix_time::ptime modif_date =
                ParseDateTime(utility::conversions::to_utf8string(date_string));

    if (file_obj.at(U("is_dir")).as_bool()) {
        p_cfile.reset(new CFolder(path, modif_date));
    } else {
       int64_t size = file_obj.at(U("bytes")).as_number().to_int64();
       string_t mime_type = file_obj.at(U("mime_type")).as_string();
        p_cfile.reset(new CBlob(path, size, mime_type, modif_date));
    }
    return p_cfile;
}

std::shared_ptr<CFolderContent> Dropbox::ListFolder(const CFolder& folder) {
    return ListFolder(folder.path());
}

bool Dropbox::CreateFolder(const CPath& path) {
    RequestInvoker ri = GetApiRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    try {
        p_retry_strategy_->InvokeRetry([&] {
            string_t url = BuildApiUrl(U("fileops/create_folder"));
            web::http::http_request request(web::http::methods::POST);
            request.set_request_uri(url);
            FormBodyBuilder fbb;
            fbb.AddParameter(U("root"), scope_);
            fbb.AddParameter(U("path"), path.path_name());
            request.set_body(fbb.Build());
            request.headers().set_content_type(fbb.ContentType());
            p_response = ri.Invoke(request);
        });
        return true;
    } catch (CHttpException& e) {
        if (e.status() == 403) {
            // object already exists, check if real folder or blob:
            std::shared_ptr<CFile> p_file = GetFile(path);
            if (!p_file) {  // should not happen, as a file exists; but in case
                LOG_ERROR << "Could not determine existing file type at path "
                          << path;
            }
            if (!p_file || !p_file->IsFolder()) {
                // a blob already exists at this path: error !
                BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, false));
            }
            // Already existing folder
            return false;
        }
        // Other http exception:
        throw;
    }
}

bool Dropbox::Delete(const CPath& path) {
    RequestInvoker ri = GetApiRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    try {
        p_retry_strategy_->InvokeRetry([&] {
            string_t url = BuildApiUrl(U("fileops/delete"));
            web::uri uri = web::uri_builder(url).to_uri();
            web::http::http_request request(web::http::methods::POST);
            request.set_request_uri(uri);
            FormBodyBuilder fbb;
            fbb.AddParameter(U("root"), scope_);
            fbb.AddParameter(U("path"), path.path_name());
            request.set_body(fbb.Build());
            request.headers().set_content_type(fbb.ContentType());

            p_response = ri.Invoke(request);
        });
        return true;
    } catch (CFileNotFoundException e) {
        // Non existing file
        return false;
    }
}

std::shared_ptr<CFile> Dropbox::GetFile(const CPath& path) {
    std::shared_ptr<CFile> p_no_file;

    RequestInvoker ri = GetApiRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    try {
        p_retry_strategy_->InvokeRetry([&] {
            string_t url = BuildFileUrl(kMetadata, path);
            web::uri uri = web::uri_builder(url)
                                .append_query(U("list"),
                                              U("false")).to_uri();
            web::http::http_request request(web::http::methods::GET);
            request.set_request_uri(uri);
            p_response = ri.Invoke(request);
        });
    } catch (CFileNotFoundException&) {
        return p_no_file;
    }
    web::json::value json = p_response->AsJson();
    if (JsonForKey(json, U("is_deleted"), false)) {
        // File is logically deleted
        LOG_DEBUG << "CFile " << path <<" is deleted";
        return p_no_file;
    }

    return ParseCFile(json.as_object());
}

void Dropbox::Download(const CDownloadRequest& download_request) {
    CPath path = download_request.path();
    RequestInvoker ri = GetRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    try {
        p_retry_strategy_->InvokeRetry([&] {
            string_t url = BuildContentUrl(U("files"), download_request.path());
            web::uri uri = web::uri_builder(url).to_uri();
            web::http::http_request request(web::http::methods::GET);
            request.set_request_uri(uri);
            for (std::pair<string_t, string_t> header :
                                            download_request.GetHttpHeaders()) {
                request.headers().add(header.first, header.second);
            }
            p_response = ri.Invoke(request);
            std::shared_ptr<ByteSink> p_byte_sink =
                                                download_request.GetByteSink();
            p_response->DownloadDataToSink(p_byte_sink.get());
        });
    } catch (CFileNotFoundException e) {
        // We have to distinguish here between "nothing exists at that path",
        // and "a folder exists at that path" :
        std::shared_ptr<CFile> p_file = GetFile(path);
        if (!p_file) {
            throw;
        }

        if (p_file->IsFolder()) {
            BOOST_THROW_EXCEPTION(
                            CInvalidFileTypeException(p_file->path(), true));
        }
        // Should not happen : a file exists but can not be downloaded ?!
        BOOST_THROW_EXCEPTION(
            CStorageException(std::string("Not downloadable blob: ")
                                + p_file->ToString()));
    }
}

void Dropbox::Upload(const CUploadRequest& upload_request) {
    CPath path = upload_request.path();
    // Check before upload : is it a folder ? (uploading a blob to a folder
    // would work, but would rename uploaded file).
    std::shared_ptr<CFile> p_file = GetFile(path);
    if (p_file && p_file->IsFolder()) {
        BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
    }

    RequestInvoker ri = GetRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        string_t url = BuildContentUrl(U("files_put"), upload_request.path());
        web::uri uri = web::uri_builder(url).to_uri();
        web::http::http_request request(web::http::methods::PUT);
        request.set_request_uri(uri);
        // Dropbox does not support content-type nor file meta information,
        // so nothing else to configure here
        std::shared_ptr<ByteSource> p_bs = upload_request.GetByteSource();
        std::unique_ptr<std::istream> p_is = p_bs->OpenStream();
        // Adapt std::istream to asynchronous concurrency istream:
        concurrency::streams::stdio_istream<uint8_t> is_wrapper(*p_is);

        request.set_body(is_wrapper,
                         p_bs->Length(),  // content_length
                         U(""));  // content_type
        p_response = ri.Invoke(request);
        // we are not interested in response body
    });
}

}  // namespace pcs_api
