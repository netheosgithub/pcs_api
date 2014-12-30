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

#include "cpprest/json.h"

#include "pcs_api/oauth2_credentials.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"
#include "pcs_api/internal/oauth2.h"
#include "pcs_api/internal/json_utils.h"
#include "pcs_api/internal/utilities.h"

namespace pcs_api {


const char_t* OAuth2Credentials::kAccessToken =
                                            PCS_API_STRING_T("access_token");
const char_t* OAuth2Credentials::kExpiresIn = PCS_API_STRING_T("expires_in");
const char_t* OAuth2Credentials::kExpiresAt = PCS_API_STRING_T("expires_at");
const char_t* OAuth2Credentials::kTokenType = PCS_API_STRING_T("token_type");

/**
 * \brief Calculate expiration timestamp, if not defined.
 *
 * Several cases: absolute expiration time exists in json,
 * or only relative, or no expiration.
 *
 * @param json
 * @return expiration date, or null if none.
 */
static boost::posix_time::ptime
CalculateExpiresAt(const web::json::value& json) {
    boost::posix_time::ptime expires_at;  // not_a_date_time
    int64_t expires_at_s = JsonForKey(json,
                                      string_t(OAuth2Credentials::kExpiresAt),
                                      (int64_t)-1);
    if (expires_at_s > 0) {
        // An absolute expiration date is present
        expires_at = boost::posix_time::from_time_t(expires_at_s);
    } else {
        // If absolute expiration date is not in json, check for relative:
        // this happens when token is received from oauth server
        int expires_in_s = JsonForKey(json, OAuth2Credentials::kExpiresIn, -1);
        if (expires_in_s > 0) {  // some tokens never expire (ie Dropbox)
            // We take a margin to be safe: it appears that some providers
            // do NOT take any margin so token would not be early refreshed
            if (expires_in_s > 6 * 60) {  // should be always true (if not -1)
                expires_in_s -= 5 * 60;  // 5 minutes to be safe
            }
            boost::posix_time::ptime now(
                            boost::posix_time::second_clock::universal_time());
            expires_at = now + boost::posix_time::seconds(expires_in_s);
            LOG_TRACE << "OAuth2Credentials: now=" << now
                      << " expires_at_=" << expires_at;
        }
    }
    return expires_at;
}

std::unique_ptr<OAuth2Credentials> OAuth2Credentials::CreateFromJson(
                                                const web::json::value& json) {
    string_t access_token =
                        json.at(OAuth2Credentials::kAccessToken).as_string();
    boost::posix_time::ptime expire_at = CalculateExpiresAt(json);
    string_t refresh_token = JsonForKey(json,
                                        OAuth2::kRefreshToken,
                                        string_t());
    string_t token_type = JsonForKey(json,
                                     OAuth2Credentials::kTokenType,
                                     string_t());
    return std::unique_ptr<OAuth2Credentials>(
            new OAuth2Credentials(access_token,
                                  expire_at,
                                  refresh_token,
                                  token_type));
}

OAuth2Credentials::OAuth2Credentials(string_t access_token,
                   boost::posix_time::ptime expires_at,
                   string_t refresh_token,
                   string_t token_type) :
    access_token_(access_token),
    expires_at_(expires_at),
    refresh_token_(refresh_token),
    token_type_(token_type) {
}


bool OAuth2Credentials::HasExpired() const {
    std::unique_lock<std::mutex>(mutex_);
    if (expires_at_ == boost::posix_time::not_a_date_time) {
        LOG_TRACE << "HasExpired - token is not expirable";
        return false;
    }
    boost::posix_time::ptime now(
                            boost::posix_time::second_clock::universal_time());
    bool ret = now > expires_at_;
    LOG_TRACE << "HasExpired=" << ret <<" (now=" << now
              << " expires_at_=" << expires_at_ << ")";
    return ret;
}


void OAuth2Credentials::Update(const web::json::value& json) {
    std::unique_lock<std::mutex>(mutex_);
    access_token_ = json.at(kAccessToken).as_string();
    token_type_ = JsonForKey(json, OAuth2Credentials::kTokenType, token_type_);
    expires_at_ = CalculateExpiresAt(json);
    refresh_token_ = JsonForKey(json, OAuth2::kRefreshToken, refresh_token_);
}

OAuth2Credentials *OAuth2Credentials::Clone() const {
    std::unique_lock<std::mutex>(mutex_);
    return new OAuth2Credentials(access_token_,
            expires_at_,
            refresh_token_,
            token_type_);
}

std::string OAuth2Credentials::ToJsonString() const {
    std::unique_lock<std::mutex>(mutex_);
    web::json::value tmp = web::json::value::object();
    tmp[kAccessToken] = web::json::value::string(access_token_);
    if (expires_at_ != boost::date_time::not_a_date_time) {
        tmp[kExpiresAt] = web::json::value::number(
            static_cast<double>(utilities::DateTimeToTime_t(expires_at_)));
    }
    if (!refresh_token_.empty()) {
        tmp[OAuth2::kRefreshToken] = web::json::value::string(refresh_token_);
    }
    if (!token_type_.empty()) {
        tmp[kTokenType] = web::json::value::string(token_type_);
    }
    return utility::conversions::to_utf8string(tmp.serialize());
}

}  // namespace pcs_api
