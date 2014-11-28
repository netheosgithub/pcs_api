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

#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/multipart_streambuf.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

MultipartStreambuf::MultipartStreambuf(MultipartStreamer *p_ms) :
    p_streamer_(p_ms), buffer_offset_(0) {
}

std::streambuf::int_type MultipartStreambuf::underflow() {
    try {
        std::streamsize nb_read = p_streamer_->ReadData(buffer_, kBufferSize);
        if (nb_read < 0) {
            // An error occured; but streambuf API can not distinguish
            // between error and EOF here !
            return traits_type::eof();
        }
        if (nb_read == 0) {
            // Standard end of stream
            return traits_type::eof();
        }
        buffer_offset_ += egptr() - eback();
        setg(buffer_, buffer_, buffer_ + nb_read);
        return traits_type::to_int_type(*gptr());
    }
    catch (...) {
        LOG_ERROR << "Exception reading multipart request body: "
                  << CurrentExceptionToString();
        return traits_type::eof();
    }
}

std::streampos MultipartStreambuf::seekpos(std::streampos sp,
                                           std::ios_base::openmode which) {
    if (sp) {
        LOG_ERROR << "Invalid attempt to seek at pos=" << sp;
        return std::streampos(std::streamoff(-1));
    }
    p_streamer_->Reset();
    buffer_offset_ = 0;
    setg(0, 0, 0);
    return sp;
}

std::streampos MultipartStreambuf::seekoff(std::streamoff off,
                                           std::ios_base::seekdir way,
                                           std::ios_base::openmode which) {
    // We only allow seek to current pos requests:
    if (off != std::streamoff(0)
        || way != std::ios_base::cur
        || which != std::ios_base::in) {
        LOG_ERROR << "Invalid attempt to seek MultipartStreambuf with off="
                  << off;
        return std::streampos(std::streamoff(-1));
    }
    return buffer_offset_ + gptr() - eback();
}


}  // namespace pcs_api
