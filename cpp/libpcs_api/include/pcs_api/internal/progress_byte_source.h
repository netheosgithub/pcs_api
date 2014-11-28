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

#ifndef INCLUDE_PCS_API_INTERNAL_PROGRESS_BYTE_SOURCE_H_
#define INCLUDE_PCS_API_INTERNAL_PROGRESS_BYTE_SOURCE_H_

#include "boost/iostreams/filtering_stream.hpp"
#include "boost/iostreams/concepts.hpp"

#include "pcs_api/byte_source.h"
#include "pcs_api/progress_listener.h"

namespace pcs_api {

namespace detail {

/**
 * \brief Utility class, part of boost filtering stream pipeline.
 *
 * The boost filter needs to be seekable since cpprestsdk calls
 * getpos(std::ios_base::in) before reading request body, in case request has
 * to be reissued (ex: digest auth 401).
 * We do not honor real seeks however.
 */
typedef boost::iostreams::multichar_filter<boost::iostreams::input_seekable>
                                               multichar_input_seekable_filter;
class ProgressInputFilter : public multichar_input_seekable_filter {
 public:
    /**
     * @param p_pl Only referenced, not owned by this object
     */
    explicit ProgressInputFilter(ProgressListener* p_pl);

    template<typename SeekableDevice>
    std::streamsize read(SeekableDevice& src,  // NOLINT
                         char* s,
                         std::streamsize n);

    template<typename SeekableDevice>
    std::streampos seek(SeekableDevice& src,  // NOLINT
                        std::streamoff offset,
                        std::ios_base::seekdir way);

    template<typename SeekableDevice> void close(SeekableDevice&);  // NOLINT

 private:
    ProgressListener* p_listener_;
    std::streamsize counter_;
    // When an exception has been thrown by listener,
    // we always reject istream reads
    bool abort_;
};

/**
 * \brief A ByteSource decorator, that reports number of read bytes
 *        to a ProgressListener.
 *
 * Bytes are actually read from a delegate ByteSource.
 */
class ProgressByteSource : public ByteSource {
 public:
    ProgressByteSource(std::shared_ptr<ByteSource> p_byte_source,
                       std::shared_ptr<ProgressListener> p_pl);
    std::unique_ptr<std::istream> OpenStream() override;
    std::streamsize Length() const override;

 private:
    std::unique_ptr<std::istream> p_source_stream_;
    std::unique_ptr<ProgressInputFilter> p_progress_filter_;
    std::shared_ptr<ByteSource> p_byte_source_;
    std::shared_ptr<ProgressListener> p_listener_;
};


}  // namespace detail

}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_INTERNAL_PROGRESS_BYTE_SOURCE_H_
