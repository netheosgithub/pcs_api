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

#include "boost/date_time/posix_time/posix_time_io.hpp"

#include "cpprest/basic_types.h"
#include "cpprest/json.h"

#include "pcs_api/password_credentials.h"
#include "pcs_api/oauth2_credentials.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"
#include "pcs_api/internal/oauth2.h"

namespace pcs_api {

std::unique_ptr<Credentials> Credentials::CreateFromJson(
                                            const std::string& json_string) {
    web::json::value json_value =
        web::json::value::parse(utility::conversions::to_string_t(json_string));
    return CreateFromJson(json_value);
}

std::unique_ptr<Credentials> Credentials::CreateFromJson(
                                            const web::json::value& json) {
    std::unique_ptr<Credentials> p_credentials;

    if (json.has_field(PasswordCredentials::kPassword)) {
        // Password credentials
        return std::unique_ptr<Credentials>(
                new PasswordCredentials(
                        json.at(PasswordCredentials::kPassword).as_string()));
    } else {
        return OAuth2Credentials::CreateFromJson(json);
    }
    return p_credentials;
}

}  // namespace pcs_api

