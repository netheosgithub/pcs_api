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

#ifndef INCLUDE_PCS_API_INTERNAL_PROGRESS_BYTE_SINK_H_
#define INCLUDE_PCS_API_INTERNAL_PROGRESS_BYTE_SINK_H_

#include "boost/iostreams/filtering_stream.hpp"
#include "boost/iostreams/concepts.hpp"

#include "pcs_api/byte_sink.h"
#include "pcs_api/progress_listener.h"

namespace pcs_api {

namespace detail {

/**
 * \brief Utility class, part of boost filtering stream pipeline.
 */
class ProgressOutputFilter : public boost::iostreams::multichar_output_filter {
 public:
    /**
     * @param p_pl Only referenced, not owned by this object
     */
    explicit ProgressOutputFilter(ProgressListener* p_pl);

    template<typename Sink>
    std::streamsize write(Sink& sink,  // NOLINT
                          const char* s,
                          std::streamsize n);

    template<typename Sink> void close(Sink& sink);  // NOLINT

 private:
    ProgressListener* p_listener_;
    std::streamsize counter_;
};

/**
 * \brief A ByteSink decorator, that reports number of written bytes to a ProgressListener.
 *
 * Bytes are actually written to a delegate ByteSink.
 */
class ProgressByteSink : public ByteSink {
 public:
    ProgressByteSink(std::shared_ptr<ByteSink> p_byte_sink,
                     std::shared_ptr<ProgressListener> p_pl);
    std::ostream* OpenStream() override;
    void CloseStream() override;
    void SetExpectedLength(std::streamsize expected_length) override;
    void Abort() override;

 private:
    typedef boost::iostreams::filtering_stream<boost::iostreams::output>
                                                                    filtstream;
    std::shared_ptr<ByteSink> p_delegate_;
    std::unique_ptr<filtstream> p_sink_stream_;
    std::unique_ptr<ProgressOutputFilter> p_progress_filter_;
    std::shared_ptr<ProgressListener> p_listener_;
};

}  // namespace detail

}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_INTERNAL_PROGRESS_BYTE_SINK_H_
