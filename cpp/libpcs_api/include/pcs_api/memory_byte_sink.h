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

#ifndef INCLUDE_PCS_API_MEMORY_BYTE_SINK_H_
#define INCLUDE_PCS_API_MEMORY_BYTE_SINK_H_

#include <string>
#include <memory>
#include <iostream>  // NOLINT(readability/streams)
#include <sstream>  // NOLINT(readability/streams)

#include "pcs_api/byte_sink.h"

namespace pcs_api {

/**
 * A sink where bytes are stored in a std::ostringstream, without limited size.
 */
class MemoryByteSink : public ByteSink {
 public:
    MemoryByteSink() {
    }
    std::ostream* OpenStream() override;
    void CloseStream() override;
    void SetExpectedLength(std::streamsize expected_length) override;
    void Abort() override;
    std::string GetData();

 private:
    std::ostringstream data_;
};



}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_MEMORY_BYTE_SINK_H_

