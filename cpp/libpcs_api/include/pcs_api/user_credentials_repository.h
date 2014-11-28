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

#ifndef INCLUDE_PCS_API_USER_CREDENTIALS_REPOSITORY_H_
#define INCLUDE_PCS_API_USER_CREDENTIALS_REPOSITORY_H_

#include <string>
#include <memory>
#include <vector>

#include "pcs_api/types.h"
#include "pcs_api/user_credentials.h"

namespace pcs_api {

/**
 * \brief A interfce used by pcs_api in order to read and persist
 *        user Credentials.
 *
 * Credentials are handled here as opaque std::string (actually json).
 */
class UserCredentialsRepository {
 public:
    /**
     * \brief Add new or replace credentials and persist.
     *
     * @param user_credentials The credentials to save (will be copied)
     */

    virtual void Save(const UserCredentials& user_credentials) = 0;

    /**
     * \brief Retrieves user credentials for the given application and
     * optional user id.
     *
     * If repository contains only one user credential for the given
     * application, user_id may be left unspecified.
     *
     * @param app_info The application informations
     * @param user_id The user identifier
     * @return The user credentials (app_info reference and user_id are copied
     *         into returned object)
     */
    virtual std::unique_ptr<UserCredentials> Get(
                                    const AppInfo& app_info,
                                    const std::string& user_id) const = 0;
    virtual ~UserCredentialsRepository() {
    }
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_USER_CREDENTIALS_REPOSITORY_H_

