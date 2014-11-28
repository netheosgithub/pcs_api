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

#include <string>

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/multipart_streamer.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

const char *kCrLf = "\r\n";

MultipartStreamer::Part::Part(const std::string& name, ByteSource *p_source) :
    name_(name), p_source_(p_source), headers_(kCrLf) {
}

void MultipartStreamer::Part::AddHeader(const std::string& name,
                                        const std::string& raw_value) {
    headers_.insert(headers_.length()-2, name + ": " + raw_value + kCrLf);
}

int64_t MultipartStreamer::Part::Length() const {
    return headers_.length() + p_source_->Length();
}

static std::string GenerateBoundary() {
    return utilities::GenerateRandomString(20);
}

MultipartStreamer::MultipartStreamer(const std::string& type) :
    MultipartStreamer(type, GenerateBoundary()) {
}

MultipartStreamer::MultipartStreamer(const std::string& type,
                                     const std::string& boundary) :
    content_type_("multipart/" + type + "; boundary=" + boundary),
    first_boundary_("--" + boundary + kCrLf),
    not_first_boundary_(kCrLf + first_boundary_),
    final_boundary_(std::string(kCrLf) + "--" + boundary + "--" + kCrLf),
    started_(false) {
}

string_t MultipartStreamer::ContentType() const {
    return utility::conversions::to_string_t(content_type_);
}

void MultipartStreamer::AddPart(const Part& part) {
    parts_.push_back(part);
    // In case a part is added during streaming
    // (unlikely, but parts_iterator_ is invalidated)
    Reset();
}

int64_t MultipartStreamer::ContentLength() const {
    int64_t size = 0;
    bool first_part = true;
    for (auto it = parts_.cbegin(); it != parts_.cend(); ++it) {
        // Count boundary:
        size += first_part ? first_boundary_.length()
                           : not_first_boundary_.length();
        first_part = false;
        // Count part:
        size += it->Length();
    }
    // add final boundary:
    size += final_boundary_.length();
    return size;
}

void MultipartStreamer::Reset() {
    started_ = false;
    p_source_stream_.reset();
}

std::streamsize MultipartStreamer::ReadData(char *p_data,
                                            std::streamsize size) {
    if (!started_) {
        // Some initialization:
        offset_ = 0;
        parts_iterator_ = parts_.begin();
        if (parts_iterator_ != parts_.end()) {
            part_state_ = kInPartFirstBoundary;
            length_ = first_boundary_.length();
        } else {
            // very unusual case: there are no part ?!
            length_ = final_boundary_.length();
        }
        started_ = true;
    }
    std::streamsize total_read = 0;
    // We keep reading chunks until client buffer is full:
    bool reached_eof = false;
    while (total_read < size && !reached_eof) {
        std::streamsize nb_read;
        if (parts_iterator_ != parts_.end()) {
            nb_read = ReadDataFromPart(p_data + total_read, size - total_read);
        } else {
            nb_read = ReadDataFromFinalBoundary(p_data + total_read,
                                                size - total_read);
            if (nb_read == 0) {
                reached_eof = true;
            }
        }
        if (nb_read < 0) {
            // An error occured
            return nb_read;
        }
        total_read += nb_read;
    }
    return total_read;
}

std::streamsize MultipartStreamer::ReadDataFromPart(char *p_data,
                                                    std::streamsize size) {
    const Part& part = *parts_iterator_;
    std::streamsize ret;
    int64_t remaining = length_ - offset_;
    switch (part_state_) {
        const char *p_from;
        std::streamsize nb_read;
        case kInPartFirstBoundary:
        case kInPartNotFirstBoundary:
            if (part_state_ == kInPartFirstBoundary) {
                p_from = first_boundary_.data() + offset_;
            } else {
                p_from = not_first_boundary_.data() + offset_;
            }
            if (size >= remaining) {
                // all boundary fits:
                ret = remaining;
                // short enough to fit in size_t:
                memcpy(p_data, p_from, (size_t)ret);
                // prepare next state:
                part_state_ = kInPartHeaders;
                offset_ = 0;
                length_ = part.headers().length();
            } else {
                // It does not fit completely:
                ret = size;
                // short enough to fit in size_t:
                memcpy(p_data, p_from, (size_t)ret);
                offset_ += ret;
            }
            break;

        case kInPartHeaders:
            p_from = part.headers().data() + offset_;
            if (size >= remaining) {
                // all headers fits:
                ret = remaining;
                // short enough to fit in size_t:
                memcpy(p_data, p_from, (size_t)ret);
                // prepare next state:
                p_source_stream_ = part.source()->OpenStream();
                part_state_ = kInPartContent;
                offset_ = 0;
                length_ = part.source()->Length();
            } else {
                // It does not fit completely:
                ret = size;
                // short enough to fit in size_t:
                memcpy(p_data, p_from, (size_t)ret);
                offset_ += ret;
            }
            break;

        case kInPartContent:
            // We read stream data directly into target buffer:
            // (we have to check errors in this case)
            p_source_stream_->read(p_data, size);
            nb_read = p_source_stream_->gcount();
            if (nb_read > 0) {
                ret = nb_read;
                offset_ += ret;
            } else {
                // We have not read anything: end of stream or error ?
                if (offset_ != length_) {
                    // we have not read the expected number of bytes,
                    // so an error occured
                    LOG_WARN << "Error reading byte source: "
                             << offset_ << " bytes read, expected " << length_;
                    return -1;
                }
                ret = 0;
                // prepare next state: either next part, or final boundary
                ++parts_iterator_;
                offset_ = 0;
                if (parts_iterator_ != parts_.end()) {
                    // we have other parts, prepare state:
                    part_state_ = kInPartNotFirstBoundary;
                    length_ = not_first_boundary_.length();
                } else {
                    // This was our last part, by now we'll write last boundary
                    length_ = final_boundary_.length();
                }
            }

            break;
        default:
            // should not happen, defensive code
            LOG_ERROR << "Invalid state in ReadDataFromPart(): " << part_state_;
            return -1;
    }
    return ret;
}

std::streamsize MultipartStreamer::ReadDataFromFinalBoundary(
                                                        char *p_data,
                                                        std::streamsize size) {
    const char *p_from = final_boundary_.data() + offset_;
    int64_t remaining = length_ - offset_;  // 0 when we are finished
    if (remaining == 0) {
        return 0;
    }
    std::streamsize ret;
    if (size >= remaining) {
        // all boundary fits:
        ret = remaining;
        memcpy(p_data, p_from, (size_t)ret);  // short enough to fit in size_t
        // there is no next state here:
        // we just update offset_ so that offset_ == length_ to indicate end
        offset_ += ret;
    } else {
        // It does not fit completely:
        ret = size;
        memcpy(p_data, p_from, (size_t)ret);  // short enough to fit in size_t
        offset_ += ret;
    }

    return ret;
}



}  // namespace pcs_api
