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

/*
*/

#include <sstream>

#include "pcs_api/c_exceptions.h"
#include "bad_memory_byte_source.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

BadMemoryByteSource::BadMemoryByteSource() :
    throw_at_open_stream_(true),
    MemoryByteSource(""),
    missing_bytes_(0) {
}

BadMemoryByteSource::BadMemoryByteSource(const std::string& data,
                                         size_t missing_bytes) :
    throw_at_open_stream_(false),
    MemoryByteSource(data.substr(0, data.length()-missing_bytes)),
    missing_bytes_(missing_bytes) {
}

std::unique_ptr<std::istream> BadMemoryByteSource::OpenStream() {
    if (throw_at_open_stream_) {
        // This could be a std::ios_base::failure on a FileByteSource:
        BOOST_THROW_EXCEPTION(std::runtime_error(
                        "Test exception when opening stream on a bad source"));
    }
    return MemoryByteSource::OpenStream();
}

std::streamsize BadMemoryByteSource::Length() const {
    return MemoryByteSource::Length() + missing_bytes_;
}

}  // namespace pcs_api

