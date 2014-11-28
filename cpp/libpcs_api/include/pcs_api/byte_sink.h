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

#ifndef INCLUDE_PCS_API_BYTE_SINK_H_
#define INCLUDE_PCS_API_BYTE_SINK_H_

#include <ostream>  // NOLINT(readability/streams)

namespace pcs_api {

/**
 * \brief Common interface for writing bytes into "something
 *        (a file, a memory buffer...)
 */
class ByteSink {
 public:
    /**
     * \brief Create a new stream for writing bytes.
     *
     * @return a stream that must be closed with CloseStream()
     */
    virtual std::ostream* OpenStream() = 0;

    /**
    * \brief Close currently opened stream.
    */
    virtual void CloseStream() = 0;

    /**
     * \brief Defines the number of bytes that are expected to be written
     *        to the stream.
     *
     * This value may be defined lately (after stream creation)
     * <p/>
     * Note that this length may differ from the final data size, for example
     * if bytes are appended to an already existing file.
     *
     * @param expected_length
     */
    virtual void SetExpectedLength(std::streamsize expected_length) = 0;

    /**
     * \brief Abort current sink operation on the opened stream.
     */
    virtual void Abort() = 0;

    virtual ~ByteSink() {
    }
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_BYTE_SINK_H_

