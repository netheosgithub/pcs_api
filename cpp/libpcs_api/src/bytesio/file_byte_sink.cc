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
#include <iosfwd>
#include <memory>
#include <system_error>

#include "cpprest/asyncrt_utils.h"

#include "boost/filesystem/fstream.hpp"

#include "pcs_api/file_byte_sink.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {
FileByteSink::FileByteSink(const boost::filesystem::path& path,
                           bool temp_name_during_write,
                           bool delete_on_abort)
    : path_(path),
      temp_name_during_write_(temp_name_during_write),
      delete_on_abort_(delete_on_abort),
      aborted_(false) {
}

const boost::filesystem::path FileByteSink::GetActualPath() {
    boost::filesystem::path actual = path_;
    if (temp_name_during_write_) {
        actual += ".part";
    }
    return actual;
}

std::ostream *FileByteSink::OpenStream() {
    boost::filesystem::path actual_path = GetActualPath();
    LOG_DEBUG << "In FileByteSink::OpenStream(): actual path=" << actual_path;
    p_ofstream_ = std::unique_ptr<boost::filesystem::ofstream>(
                                        new boost::filesystem::ofstream());
    p_ofstream_->open(actual_path,
                      ::std::ios_base::out | std::ios_base::binary);
    if (p_ofstream_->fail()) {
        std::system_error se(errno, std::system_category());
        const char *p_msg = se.what();
        BOOST_THROW_EXCEPTION(std::ios_base::failure(
                std::string("Could not open file: ")
                + utility::conversions::to_utf8string(path_.c_str())
                + ": " + p_msg));
    }

    return p_ofstream_.get();
}

void FileByteSink::CloseStream() {
    if (p_ofstream_) {  // close only once
        p_ofstream_->close();
        bool closed_properly = !p_ofstream_->fail();
        p_ofstream_.reset();  // and forget

        std::string close_exception_msg;
        if (!closed_properly) {
            std::system_error se(errno, std::system_category());
            const char *p_msg = se.what();
            close_exception_msg =
                std::string("Could not properly close file: ")
                + utility::conversions::to_utf8string(path_.c_str())
                + ": " + p_msg;
        }

        // Now examine what we do with this (possibly temp) file:
        boost::filesystem::path actual_path = GetActualPath();

        // Now two cases: it worked fine or it was aborted (or close failed)
        // This indicates that we probably have not written full content.
        if (!aborted_ && closed_properly) {
            // Case 1: it worked
            if (temp_name_during_write_) {
                // Everything went fine: we rename temp file to its final name
                boost::filesystem::remove(path_);
                boost::filesystem::rename(actual_path, path_);
            }
        } else {
            // Case 2: it failed somehow
            LOG_DEBUG << "Sink process did not finished normally";
            if (delete_on_abort_) {
                LOG_DEBUG << "Sink aborted: will delete sink file: "
                          << actual_path;
                boost::filesystem::remove(actual_path);
            } else if (boost::filesystem::exists(actual_path)) {
                std::streamsize actual_file_length =
                    static_cast<std::streamsize>(
                        boost::filesystem::file_size(actual_path));
                LOG_DEBUG << "Actual file length: " << actual_file_length;
                if (expected_length_ >= 0) {
                    if (actual_file_length == expected_length_) {
                        LOG_DEBUG << "Sink file is complete: " << actual_path
                                  << " (" << actual_file_length << " bytes)";
                    } else if (actual_file_length < expected_length_) {
                        LOG_DEBUG << "Sink file is too short: " << actual_path
                            << " ("
                            << actual_file_length << " bytes < "
                            << expected_length_ << " expected)";
                    } else {
                        LOG_DEBUG << "Sink file is too short: " << actual_path
                            << " (" << actual_file_length << " bytes > "
                            << expected_length_ << " expected)";
                    }
                } else {
                    // We didn't know how many bytes were expected
                    LOG_DEBUG << "Sink file is probably incomplete: "
                              << actual_path
                              << " (" << actual_file_length << " bytes)";
                }
            }
        }

        if (!close_exception_msg.empty()) {
            BOOST_THROW_EXCEPTION(std::ios_base::failure(close_exception_msg));
        }
    }
}

void FileByteSink::SetExpectedLength(std::streamsize length) {
    LOG_DEBUG << "In FileByteSink::SetExpectedLength(" << length << ")";
    expected_length_ = length;
}

void FileByteSink::Abort() {
    aborted_ = true;
}

FileByteSink::~FileByteSink() {
    if (p_ofstream_) {
        LOG_ERROR << "Destroying a FileByteSink without having closed stream !";
        try {
            CloseStream();
        }
        catch (...) {
            LOG_ERROR << CurrentExceptionToString();
        }
    }
}

}  // namespace pcs_api
