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

#include <thread>
#include <functional>
#include <random>
#include <exception>

#include "pcs_api/retry_strategy.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"
#include "pcs_api/internal/utilities.h"

namespace pcs_api {


RetryStrategy::RetryStrategy(int nb_tries_max, int first_sleep_ms) :
    nb_tries_max_(nb_tries_max),
    first_sleep_ms_(first_sleep_ms) {
}

void RetryStrategy::InvokeRetry(std::function<void()> request_func) {
    int current_tries = 0;
    while (true) {
        ++current_tries;
        try {
            if (current_tries > 1) {  // no need to log first attempt
                LOG_DEBUG << "Invocation #"
                          << current_tries << "/" << nb_tries_max_;
            }
            request_func();
            return;
        } catch (const CRetriableException& rex) {
            if (current_tries >= nb_tries_max_) {
                LOG_WARN << "Aborting invocations after "
                         <<  nb_tries_max_ << " failed attempts";
                std::exception_ptr p_cause = rex.cause();
                if (!p_cause) {
                    // This should never happen:
                    // every CRetriableException should have a cause exception
                    LOG_ERROR << "CRetriableException has no cause: "
                              << CurrentExceptionToString();
                    BOOST_THROW_EXCEPTION(
                        std::logic_error("CRetriableException without cause"));
                }
                // rethrow directly if CStorageException, otherwise wrap:
                try {
                    std::rethrow_exception(p_cause);
                }
                catch (CStorageException&) {
                    LOG_ERROR << "Will rethrow cause exception: "
                              << CurrentExceptionToString();
                    throw;
                }
                catch (...) {
                    LOG_ERROR << "Will wrap and rethrow cause exception: "
                              << CurrentExceptionToString();
                    BOOST_THROW_EXCEPTION(
                        CStorageException("Invocation failure",
                                          std::current_exception()));
                }
            }

            LOG_DEBUG << "Catching a CRetriableException: "
                    << current_tries << " out of " << nb_tries_max_
                    << " attempts (cause=" << ExceptionPtrToString(rex.cause())
                    << ")";

            Wait(current_tries, rex.delay());
            // and we'll try again
        } catch (const CStorageException&) {
            throw;
        } catch (...) {
            BOOST_THROW_EXCEPTION(CStorageException("Invocation failure",
                                                    std::current_exception()));
        }
    }
}


void RetryStrategy::Wait(int current_tries,
                         std::chrono::milliseconds opt_duration) {
    if (opt_duration.count() < 0) {
        double r = utilities::Random() + 0.5;
        opt_duration = std::chrono::milliseconds(static_cast<int>(
                first_sleep_ms_ * r * ((int64_t)1 << (current_tries-1))));
    }
    LOG_DEBUG << "Will retry request after "
              << opt_duration.count() << " millis";

    std::this_thread::sleep_for(opt_duration);
}

}  // namespace pcs_api
