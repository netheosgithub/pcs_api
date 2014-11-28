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

#include <sstream>

#include "pcs_api/memory_byte_source.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

MemoryByteSource::MemoryByteSource(const std::string& data):
    data_(data) {
}

std::unique_ptr<std::istream> MemoryByteSource::OpenStream() {
    return std::unique_ptr<std::istream>(new std::istringstream(data_));
}

std::streamsize MemoryByteSource::Length() const {
    return data_.length();
}

}  // namespace pcs_api

