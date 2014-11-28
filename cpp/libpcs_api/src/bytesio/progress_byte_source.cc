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

#include "boost/iostreams/filtering_stream.hpp"
#include "boost/iostreams/concepts.hpp"

#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/progress_byte_source.h"
#include "pcs_api/internal/logger.h"


namespace pcs_api {

namespace detail {


ProgressInputFilter::ProgressInputFilter(ProgressListener* p_pl) :
    p_listener_(p_pl), counter_(0), abort_(false) {
}

template<typename SeekableDevice>
std::streamsize ProgressInputFilter::read(SeekableDevice& src,  // NOLINT
                                          char* s,
                                          std::streamsize n) {
    if (abort_) {  // Something went wrong, we stop upload
        return EOF;
    }
    std::streamsize size = boost::iostreams::read(src, s, n);
    if (size != EOF) {
        try {
            p_listener_->Progress(counter_ + size);
            counter_ += size;
        }
        catch (...) {
            // Listener throws to cancel current operation
            // we return an early EOF:
            LOG_INFO
                << "Will abort process after exception in ProgressListener: "
                << CurrentExceptionToString();
            abort_ = true;
            size = EOF;
        }
    }
    return size;
}

template<typename SeekableDevice>
std::streampos ProgressInputFilter::seek(SeekableDevice& src,  // NOLINT
                                         std::streamoff offset,
                                         std::ios_base::seekdir way) {
    if (offset != 0 || way != std::ios_base::cur) {
        LOG_ERROR << "Attempt to seek with offset=" << offset
                  << " and way=" << way << " is not supported";
        BOOST_THROW_EXCEPTION(std::logic_error(
                    "Cannot seek to offset other than 0 or way not current"));
    }
    // FIXME for now we always return 0 (this is always the case in pcs_api)
    return 0;
}

template<typename SeekableDevice>
void ProgressInputFilter::close(SeekableDevice&) {
    // called during filter destruction
}


ProgressByteSource::ProgressByteSource(
        std::shared_ptr<ByteSource> p_byte_source,
        std::shared_ptr<ProgressListener> p_pl)
    : p_byte_source_(p_byte_source), p_listener_(p_pl) {
}

std::unique_ptr<std::istream> ProgressByteSource::OpenStream() {
    // We build a boost filter stream, inserting a ProgressFilter in chain
    // and underlying stream last:
    typedef boost::iostreams::filtering_stream< boost::iostreams::input>
                                                                    filtstream;
    std::unique_ptr<filtstream> p_stream =
                                std::unique_ptr<filtstream>(new filtstream());
    p_progress_filter_.reset(new ProgressInputFilter(p_listener_.get()));

    // We set buffer size to 1024 here (default 128 is too small)
    // this value defines how often progress listener is notified
    p_stream->push(*p_progress_filter_.get(), 1024, 0);
    p_source_stream_ = p_byte_source_->OpenStream();
    p_stream->push(*p_source_stream_.get());
    std::unique_ptr<std::istream> p_ret =
                            std::unique_ptr<std::istream>(p_stream.release());

    // Notify our listener that operation is starting:
    p_listener_->SetProgressTotal(Length());
    p_listener_->Progress(0);

    return p_ret;
}

std::streamsize ProgressByteSource::Length() const {
    return p_byte_source_->Length();
}

}  // namespace detail

}  // namespace pcs_api
