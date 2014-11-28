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

#ifndef INCLUDE_PCS_API_INTERNAL_REQUEST_INVOKER_H_
#define INCLUDE_PCS_API_INTERNAL_REQUEST_INVOKER_H_

#include <string>
#include <functional>

#include "cpprest/http_client.h"

#include "pcs_api/c_path.h"
#include "pcs_api/internal/c_response.h"

namespace pcs_api {

/**
 * \brief Internal object in charge of performing a http request and validating
 *        the http response.
 *
 * Request execution is actually delegated to a user function (that may be in
 * charge of adding token or user credentials before sending:
 * see OAuth2SessionManager, PasswordSessionManager).
 * If request can not be performed (ex: web::http::http_exception) the exception
 * thrown is wrapped into a CRetriableException.
 *
 * Response validation is also delegated to a user function (because this is
 * provider dependent).
 * The validation function must throw if server response is not the one
 * expected (status 2xx).
 * In case of temporary server error reponses (ex: 503), a CRetriableException
 * must be thrown.
 *
 * Note that objects passed as arguments to validate_function may be destroyed
 * after this function returns, so the validation function must act according
 * to this rule.
 */
class RequestInvoker {
 public:
    typedef std::function<std::shared_ptr<CResponse>(
                           web::http::http_request request)> request_function;
    typedef std::function<void(CResponse*, const CPath* p_opt_path)>
                                                             validate_function;

    RequestInvoker(request_function req_func,
                   validate_function validate_func,
                   const CPath* p_opt_path);
    /**
     * Performs and validates http request, then wraps http_response into
     * CResponse object.
     *
     * Two steps:<ol>
     * <li>requests server with request_func()</li>
     * <li>validates response with validate_func()</li>
     * </ol>
     *
     * @throws any kind of exception (wrapped into a CRetriableException if
     *         request should be retried).
     */
    std::shared_ptr<CResponse> Invoke(web::http::http_request request);

 private:
    web::http::client::http_client *p_client_;
    const request_function request_func_;
    const validate_function validate_func_;
    const CPath* p_path_;  // optional (may be nullptr)
    bool IsRetriable(std::exception_ptr e);
};


}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_INTERNAL_REQUEST_INVOKER_H_

