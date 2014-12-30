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

#include <memory>

#include "cpprest/json.h"

#include "pcs_api/oauth2_app_info.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {


OAuth2AppInfo::OAuth2AppInfo(std::string provider_name,
                             std::string app_name,
                             std::string app_id,
                             std::string app_secret,
                             std::vector<std::string> scope,
                             std::string redirect_url) :
        AppInfo(provider_name, app_name),
        app_id_(app_id),
        app_secret_(app_secret),
        scope_(scope),
        redirect_url_(redirect_url) {
}

const OAuth2AppInfo& OAuth2AppInfo::AsOAuth2AppInfo() const {
    return static_cast<const OAuth2AppInfo&>(*this);
}

const std::string OAuth2AppInfo::ToString() const {
    return std::string("OAuth2AppInfo{")
           +"provider_name='" + provider_name_ + '\''
           + ", app_name='" + app_name_ + '\''
           + ", app_id='" + app_id_ + '\''
           + ", redirect_url='" + redirect_url_ + "'}";
}


}  // namespace pcs_api
