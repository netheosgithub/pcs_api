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


#include "fixed_buffer_byte_sink.h"

namespace pcs_api {

FixedBufferByteSink::FixedBufferByteSink(size_t size) :
    p_buffer_(new char[size]),
    buffer_size_(size),
    data_(::std::ios_base::out | std::ios_base::binary) {
}

std::ostream* FixedBufferByteSink::OpenStream() {
    data_.buffer(p_buffer_.get(), buffer_size_);
    return &data_;
}

void FixedBufferByteSink::CloseStream() {
}

void FixedBufferByteSink::SetExpectedLength(std::streamsize expected_length) {
}

void FixedBufferByteSink::Abort() {
}

std::string FixedBufferByteSink::GetData() {
    return std::string(&p_buffer_[0], &p_buffer_[0] + (size_t)data_.tellp());
}

}  // namespace pcs_api


