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

#ifndef INCLUDE_PCS_API_OAUTH2_APP_INFO_H_
#define INCLUDE_PCS_API_OAUTH2_APP_INFO_H_

#include <string>
#include <memory>
#include <vector>

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "pcs_api/app_info.h"

namespace pcs_api {

/**
 * \brief Specialization of AppInfo for an OAuth2 application.
 */
struct OAuth2AppInfo : public AppInfo {
 public:
    OAuth2AppInfo(std::string provider_name,
            std::string app_name,
            std::string app_id,
            std::string app_secret,
            std::vector<std::string> scope,
            std::string redirect_url);
    const OAuth2AppInfo& AsOAuth2AppInfo() const override;

    const std::string& app_id() const {
        return app_id_;
    }
    const std::string& app_secret() const {
        return app_secret_;
    }
    const std::vector<std::string>& scope() const {
        return scope_;
    }
    const std::string& redirect_url() const {
        return redirect_url_;
    }
    const std::string ToString() const override;

 private:
    std::string app_id_;
    std::string app_secret_;
    std::vector<std::string> scope_;
    std::string redirect_url_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_OAUTH2_APP_INFO_H_

