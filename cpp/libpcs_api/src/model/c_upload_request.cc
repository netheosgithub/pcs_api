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

#include "pcs_api/c_upload_request.h"
#include "pcs_api/internal/progress_byte_source.h"

namespace pcs_api {

CUploadRequest::CUploadRequest(CPath path,
                               std::shared_ptr<ByteSource> p_byte_source)
    : path_(path), p_byte_source_(p_byte_source) {
}

/**
 * \brief Set the content type.
 *
 * @param contentType The content type (ie. "image/jpeg")
 * @return The upload request
 */
CUploadRequest& CUploadRequest::set_content_type(string_t content_type) {
    content_type_ = content_type;
    return *this;
}

/**
 * \brief Defines an object that will be notified during upload.
 *
 * Callback is called from an undeterminate thread (this thread may even change
 * in between calls) so care must be taken.
 *
 * @param pl progress listener Only a reference is kept.
 * @return The upload request
 */
CUploadRequest& CUploadRequest::set_progress_listener(
                                    std::shared_ptr<ProgressListener> p_pl) {
    p_listener_ = p_pl;
    return *this;
}

std::shared_ptr<ByteSource> CUploadRequest::GetByteSource() const {
    if (!p_listener_) {
        return p_byte_source_;
    } else {
        return std::make_shared<detail::ProgressByteSource>(p_byte_source_,
                                                            p_listener_);
    }
}

}  // namespace pcs_api
