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

#ifndef INCLUDE_PCS_API_INTERNAL_RETRY_401_ONCE_RESPONSE_VALIDATOR_H_
#define INCLUDE_PCS_API_INTERNAL_RETRY_401_ONCE_RESPONSE_VALIDATOR_H_

#include "pcs_api/internal/oauth2_session_manager.h"
#include "pcs_api/internal/request_invoker.h"

namespace pcs_api {

/**
 * \brief This special class is for refreshing access_token once if we get an
 * authentication error (it seems to happen that sometimes providers returns
 * spurious 401 http status).
 * This kind of error is very unlikely since we have a time margin (refreshing
 * tokens several minutes before they expire), but this error can also occur
 * if provider encounters some transient token validation error.
 *
 * This class acts as a response validation function, with a state: the first
 * 401 response is considered as retriable.
 * This class only handles 401 response, other responses are delegated to
 * another function. It can thus be seen as a validation function decorator.
 */
class Retry401OnceResponseValidator {
 public:
    Retry401OnceResponseValidator(OAuth2SessionManager *p_session_manager,
                 RequestInvoker::validate_function p_provider_validation_func);
    void ValidateResponse(CResponse *p_response, const CPath* p_opt_path);

 private:
    OAuth2SessionManager *p_session_manager_;
    RequestInvoker::validate_function p_provider_validation_func_;
    bool already_refreshed_token_ = false;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_RETRY_401_ONCE_RESPONSE_VALIDATOR_H_
