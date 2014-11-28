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

#ifndef INCLUDE_PCS_API_PASSWORD_CREDENTIALS_H_
#define INCLUDE_PCS_API_PASSWORD_CREDENTIALS_H_

#include <string>
#include <memory>
#include <vector>

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "pcs_api/types.h"
#include "pcs_api/credentials.h"

namespace pcs_api {

/**
 * \brief Specialization of Credentials, holding only a password.
 */
class PasswordCredentials : public Credentials {
 public:
    static const char_t * kPassword;
    explicit PasswordCredentials(string_t password);
    string_t password() {
        return password_;
    }
    PasswordCredentials *Clone() const override;
    std::string ToJsonString() const override;
 private:
    string_t password_;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_PASSWORD_CREDENTIALS_H_

