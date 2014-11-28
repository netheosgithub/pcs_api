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

#ifndef INCLUDE_PCS_API_C_UPLOAD_REQUEST_H_
#define INCLUDE_PCS_API_C_UPLOAD_REQUEST_H_

#include "pcs_api/c_path.h"
#include "pcs_api/progress_listener.h"
#include "pcs_api/byte_source.h"


namespace pcs_api {

/**
 * \brief Object storing several information for uploading a blob.
 */
class CUploadRequest {
 public:
    CUploadRequest(CPath path, std::shared_ptr<ByteSource> p_byte_source);

    /**
     * \brief Get the destination file path where the data will be uploaded
     *
     * @return The file path
     */
    const CPath& path() const {
        return path_;
    }

    /**
     * \brief Get the file content type.
     *
     * @return The content type or null if not defined
     */
    string_t content_type() const {
        return content_type_;
    }

    /**
     * \brief Set the content type.
     *
     * @param contentType The content type (ie. "image/jpeg")
     * @return The upload request
     */
    CUploadRequest& set_content_type(string_t content_type);

    /**
     * \brief Defines an object that will be notified during upload.
     *
     * Callback is called from an undeterminate thread (this thread may even
     * change in between calls) so care must be taken.
     *
     * @param pl progress listener Only a reference is kept.
     * @return The upload request
     */
    CUploadRequest& set_progress_listener(
                                      std::shared_ptr<ProgressListener> p_pl);

    /**
     * \brief If no progress listener has been set, return the byte source set
     *        in constructor, otherwise decorate it for progress.
     *
     * @return the byte source to be used for upload operation.
     */
    std::shared_ptr<ByteSource> GetByteSource() const;

 private:
    CPath path_;
    std::shared_ptr<ByteSource> p_byte_source_;
    string_t content_type_;
    std::shared_ptr<ProgressListener> p_listener_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_UPLOAD_REQUEST_H_


