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

#ifndef INCLUDE_PCS_API_C_DOWNLOAD_REQUEST_H_
#define INCLUDE_PCS_API_C_DOWNLOAD_REQUEST_H_

#include <map>

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "pcs_api/c_path.h"
#include "pcs_api/progress_listener.h"
#include "pcs_api/byte_sink.h"


namespace pcs_api {

/**
 * \brief Object storing several information for downloading a blob.
 */
class CDownloadRequest {
 public:
    CDownloadRequest(CPath path, std::shared_ptr<ByteSink> byte_sink);

    /**
     * \brief Get the file path to download
     *
     * @return The source file path
     */
    const CPath& path() const {
        return path_;
    }

    /**
     * \brief If no progress listener has been set, return the byte sink set
     *        in constructor, otherwise decorate it for progress.
     *
     * @return the byte sink to be used for download operation.
     */
    std::shared_ptr<ByteSink> GetByteSink() const;

    /**
     * \brief Get the HTTP headers to be used for the download request.
     *
     * @return The headers
     */
    virtual std::map<string_t, string_t> GetHttpHeaders() const;

    /**
     * \brief Defines a range for partial content download.
     * 
     * Note that second parameter is a length, not an offset (this differs
     * from http header Range header raw value)
     *
     * @param offset The start offset to download the file (or negative for
     *               undefined: last length bytes)
     * @param length The strictly positive data length to download (or negative
     *               for undefined = up to end of file)
     * @return this download request
     */
    CDownloadRequest& SetRange(int64_t offset, int64_t length);

    /**
     * Defines an object that will be notified during download.
     *
     * @param pl the progress listener
     * @return this download request
     */
    CDownloadRequest& set_progress_listener(
                                       std::shared_ptr<ProgressListener> p_pl);

 private:
    CPath path_;
    std::shared_ptr<ByteSink> p_byte_sink_;
    int64_t range_offset_;
    int64_t range_length_;
    std::shared_ptr<ProgressListener> p_listener_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_DOWNLOAD_REQUEST_H_


