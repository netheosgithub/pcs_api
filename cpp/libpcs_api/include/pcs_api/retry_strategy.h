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

#ifndef INCLUDE_PCS_API_RETRY_STRATEGY_H_
#define INCLUDE_PCS_API_RETRY_STRATEGY_H_

#include <string>
#include <map>
#include <vector>
#include <chrono>

#include "pcs_api/types.h"


namespace pcs_api {

/**
 * \brief A simple class for retrying http requests.
 *
 * Big internet providers can encounter transient errors, client library
 * must be able to handle such errors by re-issuing requests.
 * This class contains default retry implementation. As it is shared by all
 * requests, it must be stateless (if derivated).
 */
class RetryStrategy {
 public:
    RetryStrategy(int nb_tries_max, int first_sleep_ms);

    /**
     * \brief Main method to be called by user of this class:
     * calls function until success, non retriable error
     * or max trials has been reached.
     *
     * @param request_func The function which executes and validate the request
     * @throws CStorageException Request execution error
     */
    virtual void InvokeRetry(std::function<void()> request_func);

 protected:
    /**
     * \brief Wait some time before retrying function call.
     *
     * @param current_tries starts at 1, up to (nb_tries_max-1) included.
     * @param opt_duration_ms if positive or null, Wait() should use this value.
     * Otherwise, Wait() should calculate how much time it waits according to
     * current_tries. Default implementation is random exponential backup.
     */
    virtual void Wait(int current_tries,
                      std::chrono::milliseconds opt_duration_ms =
                                                std::chrono::milliseconds(-1));

 private:
    const int nb_tries_max_;
    const int first_sleep_ms_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_RETRY_STRATEGY_H_


