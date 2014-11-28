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

#include <streambuf>

#include "pcs_api/memory_byte_sink.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

std::ostream* MemoryByteSink::OpenStream() {
    data_.clear();
    data_.str("");
    return &data_;
}

void MemoryByteSink::CloseStream() {
    // Nothing to close with ostringstream
}

void MemoryByteSink::SetExpectedLength(std::streamsize length) {
    // ignored for memory byte sink
}

void MemoryByteSink::Abort() {
    // Nothing to do
}

std::string MemoryByteSink::GetData() {
    return data_.str();
}

}  // namespace pcs_api

