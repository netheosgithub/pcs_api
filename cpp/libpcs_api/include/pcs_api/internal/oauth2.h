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

#ifndef INCLUDE_PCS_API_INTERNAL_OAUTH2_H_
#define INCLUDE_PCS_API_INTERNAL_OAUTH2_H_

#include "pcs_api/types.h"

namespace pcs_api {

/**
 * \brief Some OAuth2 constants definitions.
 */
class OAuth2 {
 public:
    static const char_t * kClientId;
    static const char_t * kClientSecret;
    static const char_t * kResponseType;
    static const char_t * kCode;
    static const char_t * kRedirectUri;
    static const char_t * kScope;
    static const char_t * kGrantType;
    static const char_t * kAuthorizationCode;
    static const char_t * kRefreshToken;
    static const char_t * kState;
};

}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_INTERNAL_OAUTH2_H_

