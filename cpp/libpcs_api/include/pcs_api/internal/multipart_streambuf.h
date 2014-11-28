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

#ifndef INCLUDE_PCS_API_INTERNAL_MULTIPART_STREAMBUF_H_
#define INCLUDE_PCS_API_INTERNAL_MULTIPART_STREAMBUF_H_

#include <streambuf>

#include "pcs_api/internal/multipart_streamer.h"

namespace pcs_api {

static const size_t kBufferSize = 2048;

/**
 * \brief A streambuf for reading a multipart dynamically generated body.
 *
 * Works with an underlying MultipartStreamer,
 * implements streambuf virtual methods for reading.
 * In case of read error, no exception is thrown (it would not be catched
 * during uploads, and would abort current process). An EOF is indicated,
 * cpprestsdk detects this short body (by comparing with content-length),
 * and throws an http_exception (however the root error cause is lost).
 */
class MultipartStreambuf : public std::streambuf {
 public:
    explicit MultipartStreambuf(MultipartStreamer *p_ms);
    MultipartStreambuf(const MultipartStreambuf &) = delete;
    MultipartStreambuf &operator= (const MultipartStreambuf &) = delete;

 private:
    MultipartStreamer * const p_streamer_;
    /**
     * Our input sequence
     */
    char buffer_[kBufferSize];
    /**
     * Index of beginning of input sequence in whole stream
     */
    std::streamoff buffer_offset_;
    /**
     * Read data from our streamer
     */
    std::streambuf::int_type underflow() override;
    /**
     * We only allow seeking to beginning (so that we are able to resend
     * after 401 temp error)
     */
    std::streampos seekpos(std::streampos sp,
                           std::ios_base::openmode which) override;
    /**
     * We do not allow relative seeks, except cur+0 for tellg()
     */
    std::streampos seekoff(std::streamoff off,
                           std::ios_base::seekdir way,
                           std::ios_base::openmode which) override;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_MULTIPART_STREAMBUF_H_
