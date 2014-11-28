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

#include "cpprest/basic_types.h"
#include "cpprest/json.h"

#include "pcs_api/app_info.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

AppInfo::AppInfo(std::string provider_name, std::string app_name) :
    provider_name_(provider_name),
    app_name_(app_name) {
}

const std::string AppInfo::ToString() const {
    return std::string("AppInfo{")
           +"provider_name='" + provider_name_ + '\''
           + ", app_name='" + app_name_ + "\'}";
}

const OAuth2AppInfo& AppInfo::AsOAuth2AppInfo() const {
    // default implementation throws:
    BOOST_THROW_EXCEPTION(CStorageException("Not an OAuth2 provider"));
}

AppInfo::~AppInfo() {
}


}  // namespace pcs_api

