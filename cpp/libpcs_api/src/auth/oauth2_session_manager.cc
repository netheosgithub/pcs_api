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

#include <mutex>

#include "cpprest/http_msg.h"
#include "cpprest/http_client.h"
#include "cpprest/json.h"
#include "cpprest/asyncrt_utils.h"

#include "pcs_api/storage_builder.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/oauth2_session_manager.h"
#include "pcs_api/internal/uri_utils.h"
#include "pcs_api/internal/request_invoker.h"
#include "pcs_api/internal/form_body_builder.h"
#include "pcs_api/internal/oauth2.h"
#include "pcs_api/internal/json_utils.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

OAuth2SessionManager::OAuth2SessionManager(const string_t& authorize_url,
                                           const string_t& access_token_url,
                                           const string_t& refresh_token_url,
                                           bool scope_in_authorization,
                                           char scope_perms_separator,
                                           const StorageBuilder& builder):
            authorize_url_(authorize_url),
            access_token_url_(access_token_url),
            refresh_token_url_(refresh_token_url),
            scope_in_authorization_(scope_in_authorization),
            scope_perms_separator_(scope_perms_separator),
            app_info_(builder.GetAppInfo().AsOAuth2AppInfo()),
            p_user_credentials_repo_(builder.user_credentials_repository()),
            p_user_credentials_(builder.GetUserCredentials()),
            p_http_client_config_(builder.http_client_config()),
            p_refresh_lock_(new std::mutex()) {
    // Check type of credentials, if provided:
    std::shared_ptr<UserCredentials> p_user_creds =
                                                builder.GetUserCredentials();
    if (p_user_creds) {
        if (dynamic_cast<OAuth2Credentials *>(&p_user_creds->credentials())
                == nullptr) {
            BOOST_THROW_EXCEPTION(CStorageException(
                    "Invalid credentials type (expected OAuth2Credentials)"));
        }
    }
}

static void ThrowCStorageException(CResponse *p_response,
                                   std::string msg,
                                   bool throw_retriable = false) {
    if (p_response->IsJsonContentType()) {
        web::json::value json = p_response->AsJson();
        LOG_DEBUG << "OAuth error json response = "
                  << utility::conversions::to_utf8string(json.serialize());
        if (json.is_object()) {
            string_t oauth_error = JsonForKey(json,
                                              U("error"),
                                              string_t(U("")));
            // error is mandatory according to OAuth2 RFC, but in case:
            if (!oauth_error.empty()) {
                msg = utility::conversions::to_utf8string(oauth_error);
            }
            string_t oauth_error_description =
                                    JsonForKey(json,
                                              U("error_description"),
                                              string_t(U("")));  // optional
            if (!oauth_error_description.empty()) {
                msg += " ("
                        + utility::conversions::to_utf8string(
                            oauth_error_description)
                        + ")";
            }
        }
    } else if (msg.empty()) {
        // No message has been given, and server answered something no json:
        // in this case we take the status line as message (better than empty)
        msg = std::to_string(p_response->status())
                + " "
                + utility::conversions::to_utf8string(p_response->reason());
    }
    if (!throw_retriable) {
        p_response->ThrowCStorageException(msg, nullptr);
    }
    try {
        p_response->ThrowCStorageException(msg, nullptr);
    }
    catch (const std::exception&) {
        BOOST_THROW_EXCEPTION(CRetriableException(std::current_exception()));
    }
}

void ValidateOAuthApiResponse(CResponse *p_response, const CPath* not_used) {
    LOG_DEBUG << "Validating OAuth response: " << p_response->ToString();

    if (p_response->status() >= 500) {
        ThrowCStorageException(p_response, "", true);  // retriable
    }
    if (p_response->status() >= 300) {
        ThrowCStorageException(p_response, "");
    }
    // Server response looks correct ; however check content type is json:
    p_response->EnsureContentTypeIsJson(true);
    // one shoud also ensure it is a json object; CResponse must keep json then

    // OK, response looks fine
}

RequestInvoker OAuth2SessionManager::GetOAuthRequestInvoker() {
    return RequestInvoker(std::bind(&OAuth2SessionManager::RawExecute,
                                    this,
                                    std::placeholders::_1),  // do_request_func
                          std::bind(&ValidateOAuthApiResponse,
                                    std::placeholders::_1,
                                    std::placeholders::_2),  // validate_func
                          nullptr);
}

void OAuth2SessionManager::RefreshToken() {
    if (refresh_token_url_.empty()) {
        throw CStorageException("Provider does not support token refresh");
    }

    OAuth2Credentials& oauth_creds = static_cast<OAuth2Credentials&>(
                                        p_user_credentials_->credentials());
    const string_t before_lock_access_token = oauth_creds.access_token();

    // End of this method locks refresh lock
    // so that only one thread refreshes token at a time
    std::lock_guard<std::mutex> refresh_lock_guard(*p_refresh_lock_);

    if (oauth_creds.access_token() != before_lock_access_token) {
        // credentials have changed after lock:
        // indicates that another thread has refreshed token
        // during our wait for lock
        LOG_DEBUG << "Not refreshed token in this thread, already done";
        return;
    }
    LOG_DEBUG << "Refreshing token";

    std::shared_ptr<CResponse> p_response;
    RequestInvoker ri = GetOAuthRequestInvoker();
    // FIXME p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request post(web::http::methods::POST);
        post.set_request_uri(web::uri(access_token_url_));
        FormBodyBuilder fbb;
        fbb.AddParameter(
            OAuth2::kClientId,
            utility::conversions::to_string_t(app_info_.app_id()));
        fbb.AddParameter(
            OAuth2::kClientSecret,
            utility::conversions::to_string_t(app_info_.app_secret()));
        fbb.AddParameter(OAuth2::kRefreshToken, oauth_creds.refresh_token());
        fbb.AddParameter(OAuth2::kScope, GetScopeForAuthorization());
        fbb.AddParameter(OAuth2::kGrantType, OAuth2::kRefreshToken);
        post.set_body(fbb.Build());
        post.headers().set_content_type(fbb.ContentType());

        p_response = ri.Invoke(post);
    // });

    web::json::value json_value = p_response->AsJson();
    oauth_creds.Update(json_value);
    p_user_credentials_repo_->Save(*p_user_credentials_);
}

std::shared_ptr<UserCredentials> OAuth2SessionManager::FetchUserCredentials(
                                                        const string_t& code) {
    std::shared_ptr<CResponse> p_response;
    RequestInvoker ri = GetOAuthRequestInvoker();
    // FIXME p_retry_strategy_->InvokeRetry([&] {
        web::http::http_request post(web::http::methods::POST);
        post.set_request_uri(web::uri(access_token_url_));
        FormBodyBuilder body_builder;
        body_builder.AddParameter(
            OAuth2::kClientId,
            utility::conversions::to_string_t(app_info_.app_id()));
        body_builder.AddParameter(
            OAuth2::kClientSecret,
            utility::conversions::to_string_t(app_info_.app_secret()));
        body_builder.AddParameter(OAuth2::kCode, code);
        body_builder.AddParameter(OAuth2::kGrantType,
                                  OAuth2::kAuthorizationCode);
        if (!app_info_.redirect_url().empty()) {
            body_builder.AddParameter(
                OAuth2::kRedirectUri,
                utility::conversions::to_string_t(app_info_.redirect_url()));
        }
        post.set_body(body_builder.Build());
        post.headers().set_content_type(body_builder.ContentType());

        // Retrieve tokens:
        p_response = ri.Invoke(post);
    // });

    web::json::value json = p_response->AsJson();
    LOG_DEBUG << "FetchUserCredentials - json: "
              << utility::conversions::to_utf8string(json.serialize());
    std::unique_ptr<Credentials> p_credentials =
                                            Credentials::CreateFromJson(json);
    p_user_credentials_ = std::make_shared<UserCredentials>(
                                app_info_,
                                "",  // userId is unknown yet
                                *p_credentials);  // is copied in constructor
    return p_user_credentials_;
}

const string_t OAuth2SessionManager::GetScopeForAuthorization() const {
    if (!scope_in_authorization_) {
        return string_t();
    }

    std::string ret;
    for (std::string perm : app_info_.scope()) {
        if (ret.length() > 0) {
            ret += scope_perms_separator_;
        }
        ret +=  perm;
    }
    return utility::conversions::to_string_t(ret);
}

std::shared_ptr<CResponse> OAuth2SessionManager::Execute(
                                            web::http::http_request request) {
    LOG_TRACE << utility::conversions::to_utf8string(request.method())
              << ": "
              << utility::conversions::to_utf8string(
                    request.request_uri().to_string());

    if (!p_user_credentials_) {
        BOOST_THROW_EXCEPTION(std::logic_error(
                                "No user credentials available"));
    }
    OAuth2Credentials& oauth_creds = static_cast<OAuth2Credentials&>(
                                            p_user_credentials_->credentials());
    if (oauth_creds.HasExpired()) {
        RefreshToken();
    }

    // request object is always new even if we retry,
    // so no need to remove any old authorization header
    request.headers().add(web::http::header_names::authorization,
                          U("Bearer ") + oauth_creds.access_token());
    return RawExecute(request);
}

std::shared_ptr<CResponse> OAuth2SessionManager::RawExecute(
                                            web::http::http_request request) {
    // We do not handle exceptions at this level;
    // RequestInvoker will examine them.
    // Instantiate client now (will be given to CResponse)
    std::unique_ptr<web::http::client::http_client> p_client(
        new web::http::client::http_client(request.request_uri().authority(),
                                           *p_http_client_config_.get()));
    // will be copied when given to CResponse:
    pplx::cancellation_token_source cancel_source;
    web::http::http_response response = p_client->request(
                                            request,
                                            cancel_source.get_token()).get();
    auto p_response = std::make_shared<CResponse>(
        p_client.get(),
        std::bind(&OAuth2SessionManager::ReleaseClient,
                  this,
                   std::placeholders::_1),
        request, &response, cancel_source);
    p_client.release();  // now owned by p_response
    return p_response;
}

void OAuth2SessionManager::ReleaseClient(
                                    web::http::client::http_client *p_client) {
    // This is how we release OAuth2 clients: we destroy them
    // (we might pool them instead, as does PasswordSessionManager ?)
    delete p_client;
}

}  // namespace pcs_api
