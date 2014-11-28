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

#include "pcs_api/internal/progress_byte_sink.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

namespace detail {

ProgressOutputFilter::ProgressOutputFilter(ProgressListener* p_pl) :
    p_listener_(p_pl), counter_(0) {
}

template<typename Sink>
std::streamsize ProgressOutputFilter::write(Sink& sink,  // NOLINT
                                            const char* s,
                                            std::streamsize n) {
    std::streamsize size = boost::iostreams::write(sink, s, n);
    if (size > 0) {
        counter_ += size;
        p_listener_->Progress(counter_);
    }
    return size;
}

template<typename Sink>
void ProgressOutputFilter::close(Sink&) {
    // Closed at the end of write operations
    LOG_TRACE << "In ProgressOutputFilter::close(Sink&)";
}


ProgressByteSink::ProgressByteSink(std::shared_ptr<ByteSink> p_byte_sink,
                                   std::shared_ptr<ProgressListener> p_pl) :
    p_delegate_(p_byte_sink), p_listener_(p_pl) {
}


std::ostream* ProgressByteSink::OpenStream() {
    // We build a boost filter stream, inserting a ProgressFilter in chain
    // and underlying stream last:
    p_sink_stream_.reset(new filtstream());
    p_progress_filter_.reset(new ProgressOutputFilter(p_listener_.get()));

    // We set buffer size to 1024 here (default 128 is too small)
    // this value defines how often progress listener is notified
    p_sink_stream_->push(*p_progress_filter_.get(), 1024);
    std::ostream* p_delegate_stream = p_delegate_->OpenStream();
    p_sink_stream_->push(*p_delegate_stream);

    return p_sink_stream_.get();
}


void ProgressByteSink::CloseStream() {
    p_sink_stream_->flush();
    // Important: this ensures that ProgressOutputFilter::close() is called.
    // For arbitrary filters, close() may write some suffix data:
    // as this may fail, it is important not to do that in destructor.
    p_sink_stream_->reset();
    // By now we can destroy our pipeline (_before_ closing underlying stream):
    p_sink_stream_.reset();
    p_delegate_->CloseStream();
}

void ProgressByteSink::SetExpectedLength(std::streamsize expected_length) {
    p_listener_->SetProgressTotal(expected_length);
    p_delegate_->SetExpectedLength(expected_length);
}

void ProgressByteSink::Abort() {
    p_listener_->Aborted();
    p_delegate_->Abort();
}

}  // namespace detail
}  // namespace pcs_api
