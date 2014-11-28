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

#include "pcs_api/stdout_progress_listener.h"

namespace pcs_api {

StdoutProgressListener::StdoutProgressListener(bool single_line) :
    single_line_(single_line) {
}

StdoutProgressListener::~StdoutProgressListener() {
}

void StdoutProgressListener::SetProgressTotal(std::streamsize total) {
    total_ = total;
}

void StdoutProgressListener::Progress(std::streamsize current) {
    current_ = current;
    std::cout << "Progress: " << current_ << " / " << total_ << "\r";
    if (!single_line_) {
        std::cout << "\n";
    }
    if (current == total_) {
        if (single_line_) {
            std::cout << "\r\n";
        }
        std::cout << "********* END OF PROGRESS *********" << std::endl;
    }
}

void StdoutProgressListener::Aborted() {
    is_aborted_ = true;
    std::cout << "\nProcess has been aborted" << std::endl;
}

/**
 * Indicates if the progress has been aborted
 *
 * @return true if aborted, false otherwise
 */
bool StdoutProgressListener::is_aborted() {
    return is_aborted_;
}

}  // namespace pcs_api
