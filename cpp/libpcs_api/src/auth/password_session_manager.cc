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

#include "cpprest/http_helpers.h"
#include "cpprest/json.h"
#include "cpprest/asyncrt_utils.h"

#include "pcs_api/password_credentials.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/password_session_manager.h"
#include "pcs_api/internal/uri_utils.h"
#include "pcs_api/internal/request_invoker.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

PasswordSessionManager::PasswordSessionManager(const StorageBuilder& builder,
                                               const web::uri& base_uri)
    : clients_pool_(base_uri, builder.http_client_config()) {
    // Check type of credentials, if provided:
    std::shared_ptr<UserCredentials> p_user_creds =
                                                builder.GetUserCredentials();
    if (!p_user_creds) {
        BOOST_THROW_EXCEPTION(CStorageException(
                                "No user credentials provided"));
    }
    PasswordCredentials *p_password_credentials =
            dynamic_cast<PasswordCredentials *>(&p_user_creds->credentials());
    if (p_password_credentials == nullptr) {  // cast failed ?
        BOOST_THROW_EXCEPTION(CStorageException(
                "Invalid credentials type (expected PasswordCredentials)"));
    }
    web::credentials web_credentials(
        utility::conversions::to_string_t(p_user_creds->user_id()),
        utility::conversions::to_string_t(p_password_credentials->password()));
    // We add user credentials to the given client configuration
    // (this impacts pool):
    builder.http_client_config()->set_credentials(web_credentials);
}

std::shared_ptr<CResponse> PasswordSessionManager::Execute(
                                            web::http::http_request request) {
    LOG_TRACE << utility::conversions::to_utf8string(request.method())
              << ": "
              << UriUtils::ShortenUri(request.request_uri());

    // Take a client from the pool:
    std::unique_ptr<web::http::client::http_client> p_client =
        std::unique_ptr<web::http::client::http_client>(clients_pool_.Get());
    // will be copied when given to CResponse:
    pplx::cancellation_token_source cancel_source;
    web::http::http_response response = p_client->request(
                                            request,
                                            cancel_source.get_token()).get();
    // give client to response, and function to release client:
    auto p_response = std::make_shared<CResponse>(
            p_client.get(),
            std::bind(&PasswordSessionManager::ReleaseClient,
                      this,
                      std::placeholders::_1),
            request, &response, cancel_source);
    p_client.release();
    return p_response;
}


void PasswordSessionManager::ReleaseClient(
                                web::http::client::http_client *p_client) {
    // This is how we release clients: put them back in our pool:
    clients_pool_.Put(p_client);
}

}  // namespace pcs_api
