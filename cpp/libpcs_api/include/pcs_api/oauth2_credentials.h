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

#ifndef INCLUDE_PCS_API_OAUTH2_CREDENTIALS_H_
#define INCLUDE_PCS_API_OAUTH2_CREDENTIALS_H_

#include <string>
#include <memory>
#include <vector>
#include <mutex>

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "pcs_api/types.h"
#include "pcs_api/credentials.h"

namespace pcs_api {

/**
 * \brief A specialization of Credentials, for holding OAuth2 tokens.
 *
 * This class has variable members (access_token is refreshed),
 * but is thread safe.
 */
class OAuth2Credentials : public Credentials {
 public:
    static const char_t* kAccessToken;
    static const char_t* kExpiresIn;
    static const char_t* kExpiresAt;
    static const char_t* kTokenType;
    static std::unique_ptr<OAuth2Credentials> CreateFromJson(
                                                 const web::json::value& json);
    bool HasExpired() const;
    string_t access_token() const {
        std::unique_lock<std::mutex>(mutex_);
        return access_token_;
    }

    string_t refresh_token() const {
        std::unique_lock<std::mutex>(mutex_);
        return refresh_token_;
    }

    string_t token_type() const {
        std::unique_lock<std::mutex>(mutex_);
        return token_type_;
    }

    /**
     * Update the credentials from a JSON request response
     *
     * @param json The JSON object containing the values to update
     */
    void Update(const web::json::value& json);
    OAuth2Credentials *Clone() const override;
    std::string ToJsonString() const override;

 private:
    string_t access_token_;
    boost::posix_time::ptime expires_at_;
    string_t refresh_token_;
    string_t token_type_;
    mutable std::mutex mutex_;
    OAuth2Credentials(string_t access_token,
                      boost::posix_time::ptime expires_at,
                      string_t refresh_token,
                      string_t token_type);
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_OAUTH2_CREDENTIALS_H_

