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

#ifndef INCLUDE_PCS_API_APP_INFO_H_
#define INCLUDE_PCS_API_APP_INFO_H_

#include <string>
#include <memory>
#include <vector>

#include "pcs_api/types.h"

namespace pcs_api {

struct OAuth2AppInfo;

/**
 * \brief This base structure holds application information.
 *
 * Any application (even to connect to login/password providers)
 * has an application info.
 * To be derived for OAuth2 providers
 */
struct AppInfo {
 public:
    AppInfo(std::string provider_name, std::string app_name);

    const std::string& provider_name() const {
        return provider_name_;
    }

    const std::string& app_name() const {
        return app_name_;
    }
    virtual const OAuth2AppInfo& AsOAuth2AppInfo() const;
    virtual const std::string ToString() const;
    virtual ~AppInfo();

 protected:
    std::string provider_name_;
    std::string app_name_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_APP_INFO_H_

