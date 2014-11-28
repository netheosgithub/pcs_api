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

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/c_file.h"
#include "pcs_api/c_folder_content.h"

namespace pcs_api {

CFolderContent::CFolderContent(std::unique_ptr<content_map> p_content)
    : p_content_map_(std::move(p_content)) {
}

const CFolderContent::content_map::const_iterator
CFolderContent::cbegin() const {
    return p_content_map_->cbegin();
}

const CFolderContent::content_map::const_iterator
CFolderContent::cend() const {
    return p_content_map_->cend();
}

bool CFolderContent::ContainsPath(const CPath& path) const {
    return p_content_map_->count(path) != 0;
}

std::shared_ptr<CFile> CFolderContent::GetFile(const CPath& path) const {
    content_map::const_iterator it = p_content_map_->lower_bound(path);
    if (it == p_content_map_->end() || path < it->first) {
        // path not found
        return std::shared_ptr<CFile>();
    }
    return it->second;
}


}  // namespace pcs_api

