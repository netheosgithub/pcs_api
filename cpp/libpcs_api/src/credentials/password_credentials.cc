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

#include "pcs_api/password_credentials.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

const char_t * PasswordCredentials::kPassword = PCS_API_STRING_T("password");

PasswordCredentials::PasswordCredentials(string_t password)
    : password_(password) {
}

PasswordCredentials *PasswordCredentials::Clone() const {
    return new PasswordCredentials(password_);
}

std::string PasswordCredentials::ToJsonString() const {
    web::json::value tmp = web::json::value::object();
    tmp[kPassword] = web::json::value::string(password_);
    return utility::conversions::to_utf8string(tmp.serialize());
}


}  // namespace pcs_api
