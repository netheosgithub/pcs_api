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

#include "pcs_api/internal/oauth2.h"

namespace pcs_api {

const char_t * OAuth2::kClientId = PCS_API_STRING_T("client_id");
const char_t * OAuth2::kClientSecret = PCS_API_STRING_T("client_secret");
const char_t * OAuth2::kResponseType = PCS_API_STRING_T("response_type");
const char_t * OAuth2::kCode = PCS_API_STRING_T("code");
const char_t * OAuth2::kRedirectUri = PCS_API_STRING_T("redirect_uri");
const char_t * OAuth2::kScope = PCS_API_STRING_T("scope");
const char_t * OAuth2::kGrantType = PCS_API_STRING_T("grant_type");
const char_t * OAuth2::kAuthorizationCode =
                                        PCS_API_STRING_T("authorization_code");
const char_t * OAuth2::kRefreshToken = PCS_API_STRING_T("refresh_token");
const char_t * OAuth2::kState = PCS_API_STRING_T("state");

}  // namespace pcs_api

