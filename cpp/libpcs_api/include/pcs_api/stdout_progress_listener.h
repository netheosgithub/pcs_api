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

#ifndef INCLUDE_PCS_API_STDOUT_PROGRESS_LISTENER_H_
#define INCLUDE_PCS_API_STDOUT_PROGRESS_LISTENER_H_

#include <iostream>  // NOLINT(readability/streams)

#include "pcs_api/progress_listener.h"

namespace pcs_api {

/**
 * \brief A specialization of ProgressListener, where progress is displayed
 *        to std::cout
 *
 * Mainly for development purposes.
 */
class StdoutProgressListener: public ProgressListener {
 public:
    /**
     * @param single_line if true, progress is refreshed
     *        in the same terminal line (no new line, only carriage returns).
     * Looks nice, however if progress is not finished properly,
     * last printed progress may be overprinted by next std::cout output.
     */
    explicit StdoutProgressListener(bool single_line = true);
    virtual void SetProgressTotal(std::streamsize total) override;
    virtual void Progress(std::streamsize current) override;
    virtual void Aborted() override;
    std::streamsize total() {
        return total_;
    }
    std::streamsize current() {
        return current_;
    }
    bool is_aborted();
    virtual ~StdoutProgressListener();

 private:
    const bool single_line_;
    std::streamsize total_ = -1;
    std::streamsize current_ = 0;
    bool is_aborted_ = false;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_STDOUT_PROGRESS_LISTENER_H_

