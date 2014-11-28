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

#include "pcs_api/internal/retry_401_once_response_validator.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

Retry401OnceResponseValidator::Retry401OnceResponseValidator(
    OAuth2SessionManager *p_session_manager,
    RequestInvoker::validate_function p_provider_validation_func) :
        p_session_manager_(p_session_manager),
        p_provider_validation_func_(p_provider_validation_func),
        already_refreshed_token_(false) {
}

void Retry401OnceResponseValidator::ValidateResponse(CResponse *p_response,
                                                     const CPath* p_opt_path) {
    if (p_response->status() == web::http::status_codes::Unauthorized) {
        LOG_WARN << "Got an unexpected Unauthorized 401 response";
        if (!already_refreshed_token_) {
            // If we didn't try already, get a new access_token:
            LOG_WARN << "Will refresh access_token (in case it is broken?)";
            p_session_manager_->RefreshToken();
            already_refreshed_token_ = true;
            try {
                // will throw CAuthenticationException:
                p_provider_validation_func_(p_response, p_opt_path);
            }
            catch (std::exception&) {
                // Let's retry this request with a new access token
                // (do not wait before retry):
                BOOST_THROW_EXCEPTION(
                    CRetriableException(std::current_exception(),
                                        std::chrono::milliseconds(0)));
            }
        }
    }
    // otherwise we validate as usual:
    p_provider_validation_func_(p_response, p_opt_path);
}


}  // namespace pcs_api
