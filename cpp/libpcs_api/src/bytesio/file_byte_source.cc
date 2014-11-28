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

#include <fstream>  // NOLINT(readability/streams)
#include <cstdint>
#include <system_error>

#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/file_byte_source.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

FileByteSource::FileByteSource(const boost::filesystem::path& path)
    : path_(path) {
}

std::unique_ptr<std::istream> FileByteSource::OpenStream() {
    auto p_ret = std::unique_ptr<std::istream>(
        new boost::filesystem::ifstream(path_,
                                std::ios_base::in | std::ios_base::binary));
    if (p_ret->fail()) {
        std::system_error se(errno, std::system_category());
        const char *p_msg = se.what();
        BOOST_THROW_EXCEPTION(
                std::ios_base::failure(std::string("Could not open file: ")
                + utility::conversions::to_utf8string(path_.c_str())
                + ": " + p_msg));
    }

    return p_ret;
}

std::streamsize FileByteSource::Length() const {
    return static_cast<std::streamsize>(boost::filesystem::file_size(path_));
}

}  // namespace pcs_api
