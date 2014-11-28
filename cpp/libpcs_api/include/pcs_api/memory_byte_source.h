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

#ifndef INCLUDE_PCS_API_MEMORY_BYTE_SOURCE_H_
#define INCLUDE_PCS_API_MEMORY_BYTE_SOURCE_H_

#include <string>
#include <memory>
#include <iostream>  // NOLINT(readability/streams)

#include "pcs_api/byte_source.h"

namespace pcs_api {

/**
 * \brief Implementation of a ByteSource where bytes are read from a string.
 */
class MemoryByteSource : public ByteSource {
 public:
    /**
     * 
     * @param data will be copied
     */
    explicit MemoryByteSource(const std::string& data);
    std::unique_ptr<std::istream> OpenStream() override;
    std::streamsize Length() const override;

 private:
    std::string data_;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_MEMORY_BYTE_SOURCE_H_

