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

#ifndef INCLUDE_PCS_API_FILE_BYTE_SINK_H_
#define INCLUDE_PCS_API_FILE_BYTE_SINK_H_

#include <string>
#include <memory>
#include <iostream>  // NOLINT(readability/streams)

#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"

#include "pcs_api/byte_sink.h"

namespace pcs_api {

/**
 * \brief Implementation of a ByteSink where bytes are written
 *        into a local file.
 */
class FileByteSink : public ByteSink {
 public:
    /**
     * 
     * @param path will be copied
     */
    FileByteSink(const boost::filesystem::path& path,
                 bool temp_name_during_write = false,
                 bool delete_on_abort = false);
    std::ostream* OpenStream() override;
    void CloseStream() override;
    void SetExpectedLength(std::streamsize expected_length) override;
    void Abort() override;
    boost::filesystem::path path() {
        return path_;
    }
    ~FileByteSink();

 private:
    const boost::filesystem::path path_;
    const bool temp_name_during_write_;
    const bool delete_on_abort_;
    std::streamsize expected_length_;
    bool aborted_;
    std::unique_ptr<boost::filesystem::ofstream> p_ofstream_;
    const boost::filesystem::path GetActualPath();
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_FILE_BYTE_SINK_H_

