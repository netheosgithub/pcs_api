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

#ifndef INCLUDE_PCS_API_PROGRESS_LISTENER_H_
#define INCLUDE_PCS_API_PROGRESS_LISTENER_H_

#include <iostream>  // NOLINT(readability/streams)

namespace pcs_api {

/**
 * \brief Interface for uploads/downloads monitorings.
 *
 * Beware that methods may be called from the thread sending or receiving bytes,
 * which is likely not the same as the client thread invoking storage methods,
 * so beware of I/O, locks, etc.
 */
class ProgressListener {
 public:
    ProgressListener() {
    }
    virtual void SetProgressTotal(std::streamsize total) = 0;

    /**
     * Called when observed lengthly operation has made some progress.
     * <p/>
     * Called once with current=0 to indicate process is starting.
     * Note that progress may restart from 0 (in case an upload or download
     * fails and is restarted).
     *
     * @param current number of elements processed so far
     */
    virtual void Progress(std::streamsize current) = 0;

    /**
     * Called when current operation is aborted (may be retried)
     */
    virtual void Aborted() = 0;

    virtual ~ProgressListener() {
    }
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_PROGRESS_LISTENER_H_

