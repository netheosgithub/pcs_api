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

#ifndef INCLUDE_PCS_API_INTERNAL_OAUTH2_SESSION_MANAGER_H_
#define INCLUDE_PCS_API_INTERNAL_OAUTH2_SESSION_MANAGER_H_

#include <memory>
#include <mutex>

#include "cpprest/http_msg.h"
#include "cpprest/http_client.h"

#include "pcs_api/oauth2_app_info.h"
#include "pcs_api/oauth2_credentials.h"
#include "pcs_api/user_credentials_repository.h"
#include "pcs_api/storage_builder.h"
#include "pcs_api/internal/request_invoker.h"

namespace pcs_api {

/**
 * \brief OAuth2 tokens management (workflow, refresh, persistence...)
 *        and http_request execution.
 */
class OAuth2SessionManager {
 public:
    OAuth2SessionManager(const string_t& authorize_url,
                         const string_t& access_token_url,
                         const string_t& refresh_token_url,
                         bool scope_in_authorization,
                         char scope_perms_separator,
                         const StorageBuilder& builder);

    OAuth2Credentials GetCredentials();

    /**
     * \brief Execute the given request ;
     *        authorization header is added to request.
     *
     * This is the method to use to perform standard requests to provider API,
     * once OAuth2 authorization has been done.
     * This method handles headers, tokens renewal, etc.
     *
     * @param request
     * @return a CResponse object, owning some request info and http_response
     */
    std::shared_ptr<CResponse> Execute(::web::http::http_request request);

    /**
     * \brief Execute the given request without modifying request.
     *
     * This is the method to use to perform OAuth2 authorization workflow,
     * for getting tokens.
     *
     * @param request
     * @return a CResponse object, owning client used, some request info
     *         and http_response.
     */
    std::shared_ptr<CResponse> RawExecute(::web::http::http_request request);

    /**
     * Refreshes access token after expiration (before sending request) thanks
     * to the refresh token. New access token is then stored in this manager.
     * <p/>
     * Method is synchronized so that no two threads will attempt to refresh
     * at the same time. If a locked thread sees that token has already been
     * refreshed, no refresh is attempted either.
     * <p/>
     * Not all providers support tokens refresh (ex: CloudMe).
     */
    void RefreshToken();

    /**
     * Fetches user credentials
     *
     * @param code oauth2 OTP code
     * @return The user credentials (without userId)
     */
    std::shared_ptr<UserCredentials> FetchUserCredentials(const string_t& code);

    std::shared_ptr<UserCredentialsRepository> user_credentials_repository() {
        return p_user_credentials_repo_;
    }

    const OAuth2AppInfo& app_info() const {
        return app_info_;
    }

    string_t authorize_url() const {
        return authorize_url_;
    }
    const string_t GetScopeForAuthorization() const;

 private:
    const string_t authorize_url_;
    const string_t access_token_url_;
    const string_t refresh_token_url_;
    const bool scope_in_authorization_;
    const char scope_perms_separator_;
    const OAuth2AppInfo& app_info_;
    const std::shared_ptr<UserCredentialsRepository> p_user_credentials_repo_;
    std::shared_ptr<UserCredentials> p_user_credentials_;
    const std::shared_ptr<const web::http::client::http_client_config>
                                                         p_http_client_config_;
    // Avoids two threads refreshing token at the same time:
    // (pointer for copiable)
    std::shared_ptr<std::mutex> p_refresh_lock_;

    RequestInvoker GetOAuthRequestInvoker();
    void ReleaseClient(web::http::client::http_client *p_client);

    // gtest_prod.h : FRIEND_TEST(BasicTest, TestGetUserId)
    friend class BasicTest_TestGetUserId_Test;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_OAUTH2_SESSION_MANAGER_H_

