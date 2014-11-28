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

#ifndef INCLUDE_PCS_API_CREDENTIALS_H_
#define INCLUDE_PCS_API_CREDENTIALS_H_

#include <string>
#include <memory>
#include <vector>

#include "pcs_api/types.h"

// So that we do not need to include cpprest "json.h":
namespace web {
namespace json {
    class value;
}
}

namespace pcs_api {

/**
 * \brief Base class for storing secret elements for authorizing or
 *        authenticating a request.
 *
 * Derived according to actual auth type (OAuth2Credentials,
 * PasswordCredentials).
 */
class Credentials {
 public:
    static std::unique_ptr<Credentials> CreateFromJson(
                                               const std::string& json_string);
    static std::unique_ptr<Credentials> CreateFromJson(
                                               const web::json::value& json);
    /**
     * \brief copy this object, keeping derived class.
     *
     * @return a pointer to a created deep copy of this object
     */
    virtual Credentials *Clone() const = 0;
    virtual std::string ToJsonString() const = 0;
    virtual ~Credentials() {
    }
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_CREDENTIALS_H_

