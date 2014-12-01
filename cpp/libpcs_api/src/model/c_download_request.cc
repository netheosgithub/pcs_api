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

#include "pcs_api/c_download_request.h"
#include "pcs_api/internal/progress_byte_sink.h"
#include "pcs_api/internal/logger.h"


namespace pcs_api {

CDownloadRequest::CDownloadRequest(CPath path,
                                   std::shared_ptr<ByteSink> p_byte_sink)
    : path_(path),
      p_byte_sink_(p_byte_sink),
      range_offset_(-1),
      range_length_(-1) {
}

CDownloadRequest& CDownloadRequest::set_progress_listener(
                                     std::shared_ptr<ProgressListener> p_pl) {
    p_listener_ = p_pl;
    return *this;
}

std::map<string_t, string_t> CDownloadRequest::GetHttpHeaders() const {
    std::map<string_t, string_t> headers;
    if (range_offset_ >= 0 || range_length_ > 0) {
        std::basic_ostringstream<char_t> range;
        range << PCS_API_STRING_T("bytes=");

        int64_t start;
        if (range_offset_ >= 0) {
            range << range_offset_;
            start = range_offset_;
        } else {
            start = 1;
        }
        range << PCS_API_STRING_T("-");
        if (range_length_ > 0) {
            range << (start + range_length_ - 1);
        }

        string_t header_value = range.str();
        LOG_TRACE << "Range: " << header_value;
        headers[PCS_API_STRING_T("Range")] = header_value;
    }
    return headers;
}

CDownloadRequest& CDownloadRequest::SetRange(int64_t offset, int64_t length) {
    if (length == 0) {
        // Indicate we want to download 0 bytes ?! We ignore such requests
        LOG_WARN << "Ignored range length setting of 0.";
        length = -1;
    }
    range_offset_ = offset;
    range_length_ = length;
    return *this;
}

std::shared_ptr<ByteSink> CDownloadRequest::GetByteSink() const {
    if (!p_listener_) {
        return p_byte_sink_;
    } else {
        return std::make_shared<detail::ProgressByteSink>(p_byte_sink_,
                                                          p_listener_);
    }
}

}  // namespace pcs_api
