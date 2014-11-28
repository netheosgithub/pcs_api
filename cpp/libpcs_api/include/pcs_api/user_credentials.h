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

#ifndef INCLUDE_PCS_API_USER_CREDENTIALS_H_
#define INCLUDE_PCS_API_USER_CREDENTIALS_H_

#include <string>

#include "pcs_api/app_info.h"
#include "pcs_api/credentials.h"

namespace pcs_api {

/**
 * \brief Simple structure holding application, user id, and user Credentials.
 */
struct UserCredentials {
 public:
    /**
     *
     * @param app_info only reference will be kept
     * @param user_id is copied
     * @param credentials is copied
     */
    UserCredentials(const AppInfo& app_info,
                    const std::string& user_id,
                    const Credentials& credentials);
    const AppInfo& app_info() const {
        return app_info_;
    }
    const std::string& user_id() const {
        return user_id_;
    }
    /**
     * \brief Get credentials for this user.
     *
     * @return returned object is owned by this instance.
     */
    Credentials& credentials() const {
        return *p_credentials_;
    }

    /**
     * \brief set user id (only used for bootstrapping OAuth).
     *
     * @param user_id retrived from IStorageProvider
     */
    void set_user_id(std::string user_id) {
        user_id_ = user_id;
    }

 private:
    const AppInfo& app_info_;
    std::string user_id_;
    std::unique_ptr<Credentials> p_credentials_;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_USER_CREDENTIALS_H_

