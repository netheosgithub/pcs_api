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

#ifndef INCLUDE_PCS_API_BYTE_SOURCE_H_
#define INCLUDE_PCS_API_BYTE_SOURCE_H_

#include <memory>
#include <istream>  // NOLINT(readability/streams)

namespace pcs_api {

/**
 * \brief Common interface for reading bytes from "something" (a file,
 *        a memory buffer...)
 */
class ByteSource {
 public:
    /**
     * \brief Open a new stream for reading bytes (destroyed by user)
     */
    virtual std::unique_ptr<std::istream> OpenStream() = 0;

    /**
     * 
     * @return number of bytes in this source's stream
     */
    virtual std::streamsize Length() const = 0;

    virtual ~ByteSource() {
    }
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_BYTE_SOURCE_H_

