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

#include <cmath>

#include "boost/date_time/posix_time/posix_time_io.hpp"

#include "cpprest/uri_builder.h"
#include "cpprest/interopstream.h"

#include "pcs_api/internal/providers/swift_client.h"
#include "pcs_api/internal/c_folder_content_builder.h"
#include "pcs_api/internal/json_utils.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

/**
 * The content type of directory markers (empty swift objects).
 */
static const char_t *kContentTypeDirectory = U("application/directory");

SwiftClient::SwiftClient(
    const string_t& account_endpoint,
    const string_t& auth_token,
    std::unique_ptr<RetryStrategy> p_retry_strategy,
    bool use_directory_markers,
    std::function<std::shared_ptr<CResponse>(web::http::http_request request)>
                                                    execute_request_function)
    : account_endpoint_(account_endpoint),
      auth_token_(auth_token),
      p_retry_strategy_(std::move(p_retry_strategy)),
      use_directory_markers_(use_directory_markers),
      execute_request_function_(execute_request_function) {
}

void SwiftClient::ConfigureRequest(web::http::http_request *p_request,
                                   const string_t& format) {
    // Add authentication token:
    p_request->headers().add(U("X-Auth-token"), auth_token_);
    // if format is specified, add query parameter ....&format=json
    if (!format.empty()) {
        web::uri_builder builder(p_request->request_uri());
        builder.append_query(U("format=")
                             + web::uri::encode_data_string(format));
        p_request->set_request_uri(builder.to_uri());
    }
}

std::shared_ptr<CResponse> SwiftClient::ConfigureAndExecuteRequest(
        web::http::http_request request, string_t format) {
    ConfigureRequest(&request, format);
    return execute_request_function_(request);
}

void SwiftClient::ValidateSwiftResponse(CResponse *p_response,
                                        const CPath* p_opt_path) {
    // A response is valid if server code is 2xx.
    // It is recoverable in case of server error 5xx.
    // Swift error messages are only in reason, so nothing to extract here.
    LOG_DEBUG << "Validating swift response: " << p_response->ToString();
    bool retriable = false;
    if (p_response->status() >= 500
        // too many requests:
        || p_response->status() == 498 || p_response->status() == 429) {
        retriable = true;
    }
    if (p_response->status() >= 300) {
        if (!retriable) {
            p_response->ThrowCStorageException("", p_opt_path);
        }
        try {
            p_response->ThrowCStorageException("", p_opt_path);
        }
        catch (CStorageException&) {
            BOOST_THROW_EXCEPTION(
                                CRetriableException(std::current_exception()));
        }
    }
    // OK, response looks fine
}

void SwiftClient::ValidateSwiftApiResponse(CResponse *p_response,
                                           const CPath* p_opt_path) {
    // API response is either empty, or json:
    ValidateSwiftResponse(p_response, p_opt_path);
    int64_t cl = p_response->content_length();
    if (cl > 0) {
        // empty responses cannot be json (usually content type text/html)
        p_response->EnsureContentTypeIsJson(false);
    }
    // OK, response looks fine
}

RequestInvoker SwiftClient::GetBasicRequestInvoker(const CPath& path) {
    // Request is tweaked before execution,
    // response validation is done here:
    return RequestInvoker(
        std::bind(&SwiftClient::ConfigureAndExecuteRequest,
                  this,
                  std::placeholders::_1,
                  // request uri is kept unchanged for downloads:
                  U("")),  // do_request_func
        std::bind(&SwiftClient::ValidateSwiftResponse,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2),  // validate_func
                  &path);
}

RequestInvoker SwiftClient::GetApiRequestInvoker(const CPath* p_opt_path) {
    // Request execution is delegated to external function,
    // response validation is done here:
    return RequestInvoker(
        std::bind(&SwiftClient::ConfigureAndExecuteRequest,
                  this,
                  std::placeholders::_1,
                  // request uri is changed for API requests:
                  // ask for json response format:
                  U("json")),  // do_request_func
        std::bind(&SwiftClient::ValidateSwiftApiResponse,
                  this,
                  std::placeholders::_1,
                  std::placeholders::_2),  // validate_func
        p_opt_path);
}

void SwiftClient::UseFirstContainer() {
    std::vector<string_t> containers = GetContainers();
    if (containers.empty()) {
        BOOST_THROW_EXCEPTION(
            CStorageException("Account "
                        + utility::conversions::to_utf8string(account_endpoint_)
                        + " has no container ?!"));
    }
    UseContainer(containers[0]);
    if (containers.size() > 1) {
        LOG_WARN << "Account "
                 << utility::conversions::to_utf8string(account_endpoint_)
                 << " has " << containers.size()
                 << " containers: choosing first one as current: "
                 << current_container_;
    }
}

std::shared_ptr<CFolderContent> SwiftClient::ListFolder(const CPath& path) {
    web::json::value json = ListObjectsWithinFolder(path, U("/"));
    const web::json::array& json_array = json.as_array();
    std::shared_ptr<CFile> p_file;
    if (json_array.size() == 0) {
        // List is empty ; can be caused by a really empty folder,
        // a non existing folder, or a blob
        // Distinguish the different cases :
        p_file = GetFile(path);
        if (!p_file) {  // Nothing at that path
            return std::shared_ptr<CFolderContent>();  // empty pointer
        }
        if (p_file->IsBlob()) {  // It is a blob : error !
            BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, false));
        }
        // empty existing folder: we continue (empty loop below)
    }

    CFolderContentBuilder cfcb;
    for (web::json::array::size_type i = 0; i < json_array.size(); ++i) {
        const web::json::value& val = json_array.at(i);
        const web::json::object& obj = val.as_object();
        bool detailed;
        if (val.has_field(U("subdir"))) {
            // indicates a non empty sub directory
            // There are two cases here : provider uses directory-markers,
            // or not.
            // - if yes, another entry should exist in json with more detailed
            //   informations.
            // - if not, this will be the only entry that indicates a sub
            //   folder exists, so we keep this file, but we'll memorize it
            //   only if it is not already present in our current map.
            detailed = false;
            p_file = std::make_shared<CFolder>(
                            CPath(obj.at(U("subdir")).as_string()),
                            boost::posix_time::not_a_date_time);

        } else {
            detailed = true;
            if (obj.at(U("content_type")).as_string() !=
                                                    kContentTypeDirectory) {
                p_file = std::make_shared<CBlob>(
                            CPath(obj.at(U("name")).as_string()),
                            JsonForKey(val, U("bytes"), (int64_t)-1),
                            obj.at(U("content_type")).as_string(),
                            swift_details::ParseLastModified(val));
            } else {
                p_file = std::make_shared<CFolder>(
                            CPath(obj.at(U("name")).as_string()),
                            swift_details::ParseLastModified(val));
            }
        }

        if (detailed || !cfcb.HasPath(path)) {
            // If we got a detailed file, we always store it
            // If we got only rough description,
            // we keep it only if no info already exists
            cfcb.Add(p_file->path(), p_file);
        }
    }
    return cfcb.BuildFolderContent();
}

bool SwiftClient::CreateFolder(const CPath& path) {
    std::shared_ptr<CFile> p_file = GetFile(path);
    if (p_file) {
        if (p_file->IsFolder()) {
            return false;  // folder already exists
        }
        // It is a blob: error !
        BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, false));
    }
    if (use_directory_markers_) {
        CreateIntermediateFoldersObjects(path.GetParent());
    }
    RawCreateFolder(path);
    return true;
}

bool SwiftClient::Delete(const CPath& path) {
    // Request sub-objects w/o delimiter: all sub-objects are returned
    // In case path is a blob, we'll get an empty list
    web::json::value json = ListObjectsWithinFolder(path, U(""));
    // Now delete all objects ; we start with the deepest ones so that
    // in case we are interrupted, directory markers are still present
    // Note: swift may guarantee that list is ordered,
    // but could not confirm information...
    std::list<string_t> pathnames;
    const web::json::array& array = json.as_array();
    for (web::json::array::size_type i = 0; i < array.size(); ++i) {
        const web::json::value& obj = array.at(i);
        pathnames.push_back(U("/") + obj.at(U("name")).as_string());
    }
    pathnames.sort();
    pathnames.reverse();
    // Now we also add that top-level folder (or blob) to delete :
    pathnames.push_back(path.path_name());

    bool at_least_one_deleted = false;
    for (string_t pathname : pathnames) {
        LOG_DEBUG << "deleting object at path: "
                  << utility::conversions::to_utf8string(pathname);
        CPath path(pathname);
        string_t url = GetObjectUrl(path);
        RequestInvoker ri = GetApiRequestInvoker(&path);
        std::shared_ptr<CResponse> p_response;
        try {
            p_retry_strategy_->InvokeRetry([&] {
                web::http::http_request request(web::http::methods::DEL);
                request.set_request_uri(web::uri(url));
                p_response = ri.Invoke(request);
                at_least_one_deleted = true;
            });
        }
        catch (CFileNotFoundException&) {
            // object already deleted ? continue
        }
    }
    return at_least_one_deleted;
}

std::shared_ptr<CFile> SwiftClient::GetFile(const CPath& path) {
    std::shared_ptr<CFile> p_ret;  // empty pointer for now
    std::unique_ptr<web::http::http_headers> p_headers = HeadOrNull(path);
    if (!p_headers) {
        return p_ret;
    }
    string_t content_type = p_headers->content_type();  // empty if not present
    if (content_type.empty()) {
        LOG_WARN << path.path_name_utf8() << " object has no content type ?!";
        return p_ret;
    }
    if (content_type != kContentTypeDirectory) {
        p_ret.reset( new CBlob(
                path,
                boost::lexical_cast<int64_t>(p_headers->content_length()),
                content_type,
                swift_details::ParseTimestamp(*p_headers)));
    } else {
        p_ret.reset(new CFolder(path,
                                swift_details::ParseTimestamp(*p_headers)));
    }
    return p_ret;
}

void SwiftClient::Download(const CDownloadRequest& download_request) {
    const CPath& path = download_request.path();
    string_t url = GetObjectUrl(path);

    RequestInvoker ri = GetBasicRequestInvoker(path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(url));
        for (auto kv : download_request.GetHttpHeaders()) {
            request.headers().add(kv.first, kv.second);
        }
        p_response = ri.Invoke(request);
        if (p_response->headers().content_type() == kContentTypeDirectory) {
            BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
        }
        std::shared_ptr<ByteSink> p_sink = download_request.GetByteSink();
        p_response->DownloadDataToSink(p_sink.get());
    });
}

void SwiftClient::Upload(const CUploadRequest& upload_request) {
    const CPath& path = upload_request.path();

    // Check before upload : is it a folder ?
    // (uploading a blob to a folder would work,
    //  but would hide all folder sub-files)
    std::shared_ptr<CFile> p_file = GetFile(path);
    if (p_file && p_file->IsFolder()) {
        BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, true));
    }
    if (use_directory_markers_) {
        CreateIntermediateFoldersObjects(path.GetParent());
    }
    string_t url = GetObjectUrl(path);
    RequestInvoker ri = GetBasicRequestInvoker(path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::PUT);
        request.set_request_uri(web::uri(url));

        std::shared_ptr<ByteSource> p_bs = upload_request.GetByteSource();
        std::unique_ptr<std::istream> p_bsis = p_bs->OpenStream();
        // wrap istream as asynchronous:
        concurrency::streams::stdio_istream<uint8_t> is_wrapper(*p_bsis);
        request.set_body(is_wrapper,
                         p_bs->Length(),  // content_length
                         upload_request.content_type());  // content_type

        p_response = ri.Invoke(request);
        // not interested in response body
    });
}


std::unique_ptr<web::http::http_headers> SwiftClient::HeadOrNull(
                                                        const CPath& path) {
    std::unique_ptr<web::http::http_headers> p_ret;  // empty pointer for now
    try {
        string_t url = GetObjectUrl(path);

        RequestInvoker ri = GetBasicRequestInvoker(path);
        std::shared_ptr<CResponse> p_response;
        p_retry_strategy_->InvokeRetry([&] {
            web::http::http_request request(web::http::methods::HEAD);
            request.set_request_uri(web::uri(url));
            p_response = ri.Invoke(request);
        });
        p_ret.reset(new web::http::http_headers(p_response->headers()));
    }
    catch (CFileNotFoundException ex) {
        // can happen if file does not exist
    }
    return p_ret;
}

void SwiftClient::UseContainer(string_t container_name) {
    current_container_ = container_name;
    LOG_DEBUG << "Using container: "
              << utility::conversions::to_utf8string(current_container_);
}

std::vector<string_t> SwiftClient::GetContainers() {
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(account_endpoint_));
        p_response = ri.Invoke(request);
    });
    web::json::value json = p_response->AsJson();
    const web::json::array& json_array = json.as_array();

    std::vector<string_t> containers;
    for (web::json::array::size_type i = 0; i < json_array.size(); ++i) {
        containers.push_back(json_array.at(i).at(U("name")).as_string());
    }
    LOG_DEBUG << "Available containers: " << containers.size();
    return containers;
}

void SwiftClient::RawCreateFolder(const CPath& path) {
    string_t url = GetObjectUrl(path);
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::PUT);
        request.set_request_uri(web::uri(url));
        request.headers().set_content_type(kContentTypeDirectory);
        request.headers().set_content_length(0);
        p_response = ri.Invoke(request);
    });
    // We are not interested in response body
}

void SwiftClient::CreateIntermediateFoldersObjects(
                                            const CPath& leaf_folder_path) {
    // We check for folder existence before creation,
    // as in general leaf folder is likely to already exist.
    // So we walk from leaf to root:
    CPath path = leaf_folder_path;

    std::vector<CPath> parent_folders;
    std::shared_ptr<CFile> p_file;
    while (!path.IsRoot()) {  // loop, climbing in hierarchy
        p_file = GetFile(path);
        if (p_file) {
            if (p_file->IsBlob()) {
                // Problem here: clash between folder and blob
                BOOST_THROW_EXCEPTION(CInvalidFileTypeException(path, false));
            }
            break;
        } else {
            LOG_TRACE << "Nothing exists at path: "
                      << path.path_name_utf8() << ", will go up";
            parent_folders.insert(parent_folders.begin(), path);
            path = path.GetParent();
        }
    }

    // By now we know which folders to create:
    if (!parent_folders.empty()) {
        LOG_DEBUG << parent_folders.size()
                  << " inexisting parent_folders will be created";
        for (CPath parent : parent_folders) {
            LOG_TRACE << "Creating intermediate folder: "
                      << parent.path_name_utf8();
            RawCreateFolder(parent);
        }
    }
}

web::json::value SwiftClient::ListObjectsWithinFolder(const CPath& path,
                                                      string_t opt_delimiter) {
    // prefix should not start with a slash, but end with a slash:
    // '/path/to/folder' --> 'path/to/folder/'
    string_t prefix = path.path_name().substr(1) + U("/");
    if (prefix == U("/")) {
        prefix = U("");
    }

    string_t url = GetCurrentContainerUrl();
    web::uri uri(url);
    web::uri_builder builder(uri);
    builder.append_query(U("prefix=") + web::uri::encode_data_string(prefix));
    if (!opt_delimiter.empty()) {
        builder.append_query(U("delimiter=")
                             + web::uri::encode_data_string(opt_delimiter));
    }
    uri = builder.to_uri();

    RequestInvoker ri = GetApiRequestInvoker(&path);
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(uri);
        p_response = ri.Invoke(request);
    });
    return p_response->AsJson();
}

string_t SwiftClient::GetObjectUrl(const CPath& path) {
    string_t container_url = GetCurrentContainerUrl();
    return container_url + path.GetUrlEncoded();
}

string_t SwiftClient::GetCurrentContainerUrl() {
    if (current_container_.empty()) {
        BOOST_THROW_EXCEPTION(std::logic_error(
            std::string("Undefined current container for account ")
                    + utility::conversions::to_utf8string(account_endpoint_)));
    }
    return account_endpoint_ + U("/") + current_container_;
}


namespace swift_details {

boost::posix_time::ptime ParseLastModified(const web::json::value& val) {
    boost::posix_time::ptime last_modified;  // not a datetime
    try {
        // looks like: "2014-01-15T16:37:43.427570"
        string_t modified_wstr = JsonForKey(val,
                                            U("last_modified"),
                                            string_t());
        std::string modified_str =
                            utility::conversions::to_utf8string(modified_wstr);
        if (modified_str.empty()) {
            return last_modified;
        }
        // Date is GMT, this is what boost parser expects
        boost::posix_time::time_input_facet *p_facet;
        if (modified_str.find(".") == std::string::npos) {
            // seconds only:
            p_facet =
                new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%S");
        } else {
            // with microseconds:
            p_facet =
                new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%s");
        }
        std::locale loc(std::locale::classic(), p_facet);
        std::istringstream is((modified_str));
        is.imbue(loc);
        is >> last_modified;
    }
    catch (web::json::json_exception&) {
        LOG_WARN << "Error parsing date: "
                 << utility::conversions::to_utf8string(val.serialize());
    }
    return last_modified;
}

boost::posix_time::ptime ParseTimestamp(
                                    const web::http::http_headers& headers) {
    boost::posix_time::ptime ret;  // not a date time, if we return early
    auto it = headers.find(U("X-Timestamp"));
    if (it == headers.end()) {
        return ret;
    }
    // Looks like "1408550324.34246"
    std::string header_value = utility::conversions::to_utf8string(it->second);

    // handle seconds:
    size_t index = header_value.find('.');
    int64_t seconds_since_epoch;
    if (index != std::string::npos) {
        seconds_since_epoch =
                boost::lexical_cast<int64_t>(header_value.substr(0, index));
    } else {
        // Unusual case, no fractional seconds ?
        seconds_since_epoch = boost::lexical_cast<int64_t>(header_value);
    }
    ret = boost::posix_time::from_time_t(seconds_since_epoch);

    // Handle milliseconds, if present:
    if (index != std::string::npos && index < header_value.length()-1) {
        // Add milliseconds (beware, string can be short):
        int ms = static_cast<int>(round(boost::lexical_cast<double>(
                                        header_value.substr(index)) * 1000));
        boost::posix_time::milliseconds offset(ms);
        ret += offset;
    }

    return ret;
}

}  // namespace swift_details

}  // namespace pcs_api
