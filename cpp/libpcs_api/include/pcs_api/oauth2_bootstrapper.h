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

#ifndef INCLUDE_PCS_API_OAUTH2_BOOTSTRAPPER_H_
#define INCLUDE_PCS_API_OAUTH2_BOOTSTRAPPER_H_

#include <string>
#include <memory>

#include "pcs_api/i_storage_provider.h"

namespace pcs_api {

// forward declare in order to not include private headers:
template <class T> class StorageProvider;
class OAuth2SessionManager;
typedef StorageProvider<OAuth2SessionManager> OAuth2StorageProvider;

/**
 * \brief A class for performing OAuth2 authorization code workflow.
 */
class OAuth2Bootstrapper {
 public:
    /**
     * @param p_provider (has been built with for_bootstrapping(true))
     */
    explicit OAuth2Bootstrapper(std::shared_ptr<IStorageProvider> p_provider);

    /**
     * \brief Builds the authorize URI that must be loaded in a browser to
     *        allow the application to use the API.
     *
     * @return The authorize URI
     */
    std::string GetAuthorizeBrowserUrl();

    /**
     * \brief Gets users Credentials with code workflow.
     *
     * Provider internal state (authorization tokens) is updated,
     * and tokens are persisted.
     *
     * @param code_or_url code or redirect URL provided by the browser
     */
    void GetUserCredentials(string_t code_or_url);
#if defined _WIN32 || defined _WIN64
    void GetUserCredentials(std::string code_or_url);
#endif

 private:
    std::shared_ptr<OAuth2StorageProvider> p_storage_provider_;
    string_t state_;
};

}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_OAUTH2_BOOTSTRAPPER_H_

