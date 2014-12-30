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

#include "boost/algorithm/string.hpp"

#include "cpprest/http_client.h"

#include "pcs_api/internal/request_invoker.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {


RequestInvoker::RequestInvoker(const request_function request_func,
                               const validate_function validate_func,
                               const CPath* p_opt_path) :
    request_func_(request_func),
    validate_func_(validate_func),
    p_path_(p_opt_path) {
}

std::shared_ptr<CResponse> RequestInvoker::Invoke(
                                            web::http::http_request request) {
    std::shared_ptr<CResponse> p_response;
    bool request_done = false;
    try {
        p_response = request_func_(request);  // may throw
        request_done = true;
        validate_func_(p_response.get(), p_path_);
        return p_response;
    }
    catch(std::exception&) {
        std::exception_ptr p_current = std::current_exception();
        LOG_DEBUG << "catched exception in request_invoker: "
                  << ExceptionPtrToString(p_current);
        if (request_done || !IsRetriable(p_current)) {
            // request has been done and validation failed
            // with not retriable error:
            // LOG_DEBUG << "RequestInvoker: exception rethrown !";
            throw;
        }
        // Exception is retriable:
        BOOST_THROW_EXCEPTION(CRetriableException(p_current));
    }
}

bool RequestInvoker::IsRetriable(std::exception_ptr p_ex) {
    bool ret = false;
    try {
        std::rethrow_exception(p_ex);
    }
    catch (const web::http::http_exception& he) {
        // This is usually a low level network or protocol error
        // that should be retried:
        ret = true;
        // However, error can be local (ex: when uploading a file):
        // these errors are not transient and should not be retried
        // The only way is to check exception message
        // (FIXME not robust because impl specific):
        if (boost::algorithm::starts_with(he.what(),
                                          "Error reading outgoing HTTP body")
           || boost::algorithm::starts_with(he.what(),
                       "Unexpected end of request body stream")) {
            ret = false;
        }
    }
    catch (...) {
        // other exceptions: may be any kind, so we give up
    }
    LOG_DEBUG << "IsRetriable(" << ExceptionPtrToString(p_ex)
              << ") will return " << ret;
    return ret;
}

}  // namespace pcs_api
