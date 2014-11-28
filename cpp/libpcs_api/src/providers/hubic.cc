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

#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/providers/swift_client.h"
#include "pcs_api/internal/retry_401_once_response_validator.h"
#include "pcs_api/internal/providers/hubic.h"
#include "pcs_api/internal/c_folder_content_builder.h"
#include "pcs_api/internal/storage_provider.h"
#include "pcs_api/internal/json_utils.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/form_body_builder.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

const char* Hubic::kProviderName = "hubic";

static const char_t *kRoot = U("https://api.hubic.com");
static const char_t *kEndPoint = U("https://api.hubic.com/1.0");

StorageBuilder::create_provider_func Hubic::GetCreateInstanceFunction() {
    return Hubic::CreateInstance;
}

std::shared_ptr<IStorageProvider> Hubic::CreateInstance(
                                            const StorageBuilder& builder) {
    // make_shared not usable because constructor is private
    return std::shared_ptr<IStorageProvider>(new Hubic(builder));
}


Hubic::Hubic(const StorageBuilder& builder) :
    StorageProvider(builder.provider_name(),
                    std::make_shared<OAuth2SessionManager>(
                            // authorize_url:
                            string_t(kRoot) + U("/oauth/auth/"),
                            // access_token_url:
                            string_t(kRoot) + U("/oauth/token/"),
                            // refresh_token_url:
                            string_t(kRoot) + U("/oauth/token/"),
                            true,  // scope_in_authorization,
                            ',',  // scope_perms_separator
                            builder),
                    builder.retry_strategy()) {
}

void Hubic::ThrowCStorageException(CResponse *p_response,
                                   const CPath* p_opt_path,
                                   bool throw_retriable) {
    // Try to extract error message from json body:
    // (this body can be json even if content type header is text/html !)
    // { "error":"invalid_token", "error_description":"not found" }
    std::string error_msg;
    try {
        // p_response->AsJson() fails if content type is not json
        std::string body = p_response->AsString();
        web::json::value json = web::json::value::parse(
                                    utility::conversions::to_string_t(body));
        const web::json::object& json_obj = json.as_object();

        error_msg = utility::conversions::to_utf8string(
                                        json_obj.at(U("error")).as_string());
        std::string error = utility::conversions::to_utf8string(
                            json_obj.at(U("error_description")).as_string());
        error_msg += " (" + error + ")";
    }
    catch (web::json::json_exception&) {
        // Could not parse server error msg... happens when content is pure html
    }
    catch (CStorageException ex) {
        // Could not read server error msg...
    }
    if (!throw_retriable) {
        p_response->ThrowCStorageException(error_msg, p_opt_path);
    }
    try {
        p_response->ThrowCStorageException(error_msg, p_opt_path);
    } catch (...) {
        BOOST_THROW_EXCEPTION(CRetriableException(std::current_exception()));
    }
}

/**
 * \brief Validate a response from hubiC API.
 *
 * A response is valid if server code is 2xx and content-type JSON.
 * It is recoverable in case of server error 5xx.
 */
void Hubic::ValidateHubicApiResponse(CResponse *p_response,
                                     const CPath* p_opt_path) {
    LOG_DEBUG << "Validating hubiC response: " << p_response->ToString();

    bool retriable = false;
    if (p_response->status() >= 500) {
        retriable = true;
    }
    if (p_response->status() >= 300) {
        // We can get spurious 302 responses here !? for diagnostics:
        if (p_response->status() < 400) {
            string_t location;
            if (p_response->headers().match(web::http::header_names::location,
                                            location)) {
                LOG_WARN << "Spurious redirect to URL: " << location;
                // It happens (unlikely) that response is
                // "302 Moved Temporarily" to http://hubic.com/error.xml
                // We consider these kind of errors to be retriable:
                if (location.find(U("error")) != string_t::npos) {
                    retriable = true;
                }
            }
        }
        ThrowCStorageException(p_response, p_opt_path, retriable);
    }
    // Server response looks correct ; however check content type is json:
    p_response->EnsureContentTypeIsJson(true);
    // OK, response looks fine
}

/**
 * \brief An invoker that checks response content type = json:
 *        to be used by all API requests.
 *
 * @param p_opt_path (optional) context for the request: path of remote file
 * @return a request invoker specific for API requests
 */
RequestInvoker Hubic::GetApiRequestInvoker(const CPath* p_opt_path) {
    // Request execution is delegated to our session manager,
    // response validation is done thanks to a Retry401OnceResponseValidator
    // object, ref counted so that it is deleted once no more bound
    // (hence when RequestInvoker is destroyed):
    std::shared_ptr<Retry401OnceResponseValidator> p_validator_object =
        std::make_shared<Retry401OnceResponseValidator>(
            p_session_manager_.get(),
            std::bind(&Hubic::ValidateHubicApiResponse,
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

std::string Hubic::GetUserId() {
    // user_id is email in case of hubic
    string_t url = string_t(kEndPoint) + U("/account");
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(url));
        p_response = ri.Invoke(request);
    });
    web::json::value json = p_response->AsJson();
    string_t email = json.as_object().at(U("email")).as_string();
    return utility::conversions::to_utf8string(email);
}

CQuota Hubic::GetQuota() {
    // Return a CQuota object
    string_t url = string_t(kEndPoint) + U("/account/usage");
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(url));
        p_response = ri.Invoke(request);
    });
    web::json::value json = p_response->AsJson();
    return CQuota(JsonForKey(json, U("used"), (int64_t)-1),
                  JsonForKey(json, U("quota"), (int64_t)-1));
}


/**
* \brief a fake retry strategy that actually does not retry,
*        nor handle exceptions.
*
* Used by swift client. Exceptions are handled here: in case something goes
* wrong with Swift requests, a new client is instantiated.
*/
class NoRetryStrategy : public RetryStrategy {
 public:
    NoRetryStrategy() :
        RetryStrategy(1, 0) {
    }
    void InvokeRetry(std::function<void()> request_func) override {
        request_func();
    }
};

std::shared_ptr<SwiftClient> Hubic::GetSwiftClient() {
    // Only one thread can instantiate client:
    std::lock_guard<std::mutex> lock(swift_client_mutex_);

    // take snapshot of member variable in a local variable
    // as member variable can be reset at anytime:
    std::shared_ptr<SwiftClient> p_swift = p_swift_client_;
    if (p_swift) {
        return p_swift;  // usual path
    }
    // No client yet (or has been invalidated):
    // hubiC API gives us informations for instantiation
    string_t url = string_t(kEndPoint) + U("/account/credentials");
    RequestInvoker ri = GetApiRequestInvoker();
    std::shared_ptr<CResponse> p_response;
    p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request request(web::http::methods::GET);
        request.set_request_uri(web::uri(url));
        p_response = ri.Invoke(request);
    });
    web::json::value json = p_response->AsJson();
    const web::json::object& json_obj = json.as_object();
    const string_t& swift_endpoint = json_obj.at(U("endpoint")).as_string();
    const string_t& swift_token = json_obj.at(U("token")).as_string();
    p_swift = std::make_shared<SwiftClient>(
                    swift_endpoint,
                    swift_token,
                    std::unique_ptr<RetryStrategy>(new NoRetryStrategy()),
                    true,  // use_directory_markers
                    // we delegate requests execution to our session manager:
                    std::bind(&OAuth2SessionManager::RawExecute,
                              p_session_manager_.get(),
                              std::placeholders::_1));
    p_swift->UseFirstContainer();
    p_swift_client_ = p_swift;
    return p_swift;  // return snapshot
}

void Hubic::SwiftCall(std::function<void(SwiftClient *p_swift)> user_func) {
    try {
        std::shared_ptr<SwiftClient> p_swift = GetSwiftClient();
        user_func(p_swift.get());
    }
    catch (const CAuthenticationException&) {
        LOG_WARN << "Swift authentication error: swift client invalidated";
        p_swift_client_.reset();
        // Wrap as a retriable error without wait,
        // so that retrier will not abort :
        BOOST_THROW_EXCEPTION(CRetriableException(std::current_exception(),
                                                std::chrono::milliseconds(0)));
    }
}

std::shared_ptr<CFolderContent> Hubic::ListRootFolder() {
    return ListFolder(CPath(U("/")));
}

std::shared_ptr<CFolderContent> Hubic::ListFolder(const CFolder& folder) {
    return ListFolder(folder.path());
}

std::shared_ptr<CFolderContent> Hubic::ListFolder(const CPath& path) {
    std::shared_ptr<CFolderContent> p_ret;
    p_retry_strategy_->InvokeRetry([&] {
        SwiftCall([&](SwiftClient *p_swift) {
            p_ret = p_swift->ListFolder(path);
        });
    });
    return p_ret;
}

bool Hubic::CreateFolder(const CPath& path) {
    bool ret;
    p_retry_strategy_->InvokeRetry([&] {
        SwiftCall([&](SwiftClient *p_swift) {
            ret = p_swift->CreateFolder(path);
        });
    });
    return ret;
}

bool Hubic::Delete(const CPath& path) {
    bool ret;
    p_retry_strategy_->InvokeRetry([&] {
        SwiftCall([&](SwiftClient *p_swift) {
            ret = p_swift->Delete(path);
        });
    });
    return ret;
}

std::shared_ptr<CFile> Hubic::GetFile(const CPath& path) {
    std::shared_ptr<CFile> p_ret;
    p_retry_strategy_->InvokeRetry([&] {
        SwiftCall([&](SwiftClient *p_swift) {
            p_ret = p_swift->GetFile(path);
        });
    });
    return p_ret;
}

void Hubic::Download(const CDownloadRequest& download_request) {
    p_retry_strategy_->InvokeRetry([&] {
        SwiftCall([&](SwiftClient *p_swift) {
            p_swift->Download(download_request);
        });
    });
}

void Hubic::Upload(const CUploadRequest& upload_request) {
    p_retry_strategy_->InvokeRetry([&] {
        SwiftCall([&](SwiftClient *p_swift) {
            p_swift->Upload(upload_request);
        });
    });
}


}  // namespace pcs_api
