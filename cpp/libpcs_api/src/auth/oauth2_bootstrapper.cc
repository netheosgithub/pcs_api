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

#include "boost/algorithm/string/predicate.hpp"

#include "cpprest/uri_builder.h"
#include "cpprest/uri_parser.h"

#include "pcs_api/oauth2_bootstrapper.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/storage_builder.h"
#include "pcs_api/internal/storage_provider.h"
#include "pcs_api/internal/oauth2_session_manager.h"
#include "pcs_api/internal/oauth2.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/logger.h"
#include "pcs_api/internal/uri_utils.h"

/**
 * Utility class to retrieve initial token and populate a
 * UserCredentialsRepository.
 */

namespace pcs_api {

OAuth2Bootstrapper::OAuth2Bootstrapper(
                    std::shared_ptr<IStorageProvider> p_storage_provider)
    : p_storage_provider_(std::dynamic_pointer_cast<OAuth2StorageProvider>(
                                                        p_storage_provider)) {
    if (!p_storage_provider_) {  // Dynamic cast failed
        // This class works only for oauth2 providers
        BOOST_THROW_EXCEPTION(CStorageException(
                        "This provider does not use OAuth2 authentication"));
    }
}

std::string OAuth2Bootstrapper::GetAuthorizeBrowserUrl() {
    const OAuth2AppInfo& app_info =
                        p_storage_provider_->p_session_manager_->app_info();

    state_ = utility::conversions::to_string_t(
                        utilities::GenerateRandomString(30));

    web::uri_builder builder(p_storage_provider_->
                                        p_session_manager_->authorize_url());
    builder.append_query(OAuth2::kClientId,
                         utility::conversions::to_string_t(app_info.app_id()))
        .append_query(OAuth2::kState, state_)
        .append_query(OAuth2::kResponseType, PCS_API_STRING_T("code"));
    if (!app_info.redirect_url().empty()) {
        builder.append_query(
            OAuth2::kRedirectUri,
            utility::conversions::to_string_t(app_info.redirect_url()));
    }
    string_t scope = p_storage_provider_->p_session_manager_->
                                                    GetScopeForAuthorization();
    if (!scope.empty()) {
        builder.append_query<string_t>(OAuth2::kScope, scope);
    }
    return utility::conversions::to_utf8string(builder.to_uri().to_string());
}

#if defined _WIN32 || defined _WIN64
void OAuth2Bootstrapper::GetUserCredentials(std::string code_or_url) {
    GetUserCredentials(utility::conversions::to_string_t(code_or_url));
}
#endif

void OAuth2Bootstrapper::GetUserCredentials(string_t code_or_url) {
    if (state_.empty()) {
        // should not occur if this class is used properly
        BOOST_THROW_EXCEPTION(std::logic_error("No anti CSRF state defined"));
    }

    string_t code;
    if (boost::starts_with(code_or_url,
                           PCS_API_STRING_T("http://localhost/"))
            || boost::starts_with(code_or_url,
                                  PCS_API_STRING_T("http://localhost:"))
            || boost::starts_with(code_or_url,
                                  PCS_API_STRING_T("https://"))) {
        // It's a URL: extract code and check state
        string_t url = code_or_url;
        LOG_DEBUG << "redirect URL: "<< url;
        web::uri uri(url);
        std::map<string_t, string_t> params =
                                UriUtils::ParseQueryParameters(uri.query());

        string_t error = params[PCS_API_STRING_T("error")];
        if (!error.empty()) {
            string_t error_description =
                                params[PCS_API_STRING_T("error_description")];
            std::string msg("User authorization failed: "
                            + utility::conversions::to_utf8string(error));
            if (!error_description.empty()) {
                msg += " ("
                        + utility::conversions::to_utf8string(error_description)
                        + ")";
            }
            throw CStorageException(msg);
        }

        string_t state_to_test = params[PCS_API_STRING_T("state")];
        if (state_ != state_to_test) {
            throw CStorageException(std::string("CSRF state received (")
                    + utility::conversions::to_utf8string(state_to_test)
                    + ") is not state expected ("
                    + utility::conversions::to_utf8string(state_) + ")");
        }
        code = params[PCS_API_STRING_T("code")];
        if (code.empty()) {
            throw CStorageException("Can't find code in redirected URL: "
                    + utility::conversions::to_utf8string(url));
        }
    } else {
        // It's a code
        code = code_or_url;
    }

    std::shared_ptr<UserCredentials> p_user_credentials =
            p_storage_provider_->p_session_manager_->FetchUserCredentials(code);
    // From access token we can get user id...
    std::string user_id = p_storage_provider_->GetUserId();
    LOG_DEBUG << "User identifier retrieved: " << user_id;

    // ...so that by now we can persist tokens:
    p_user_credentials->set_user_id(user_id);
    p_storage_provider_->p_session_manager_->
                    user_credentials_repository()->Save(*p_user_credentials);
}

}  // namespace pcs_api

