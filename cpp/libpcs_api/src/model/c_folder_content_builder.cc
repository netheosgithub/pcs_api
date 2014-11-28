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
#include "pcs_api/internal/c_folder_content_builder.h"

namespace pcs_api {

CFolderContentBuilder::CFolderContentBuilder()
    : p_content_map_(new CFolderContent::content_map()) {
}

bool CFolderContentBuilder::HasPath(const CPath& search) {
    return p_content_map_->find(search) != p_content_map_->end();
}

void CFolderContentBuilder::Add(const CPath& path,
                                std::shared_ptr<CFile> p_file) {
    (*p_content_map_)[path] = p_file;
}

std::shared_ptr<CFolderContent> CFolderContentBuilder::BuildFolderContent() {
    // cannot use make_shared because constructor is private
    return std::shared_ptr<CFolderContent>(
                                new CFolderContent(std::move(p_content_map_)));
}

std::ostream& operator<<(std::ostream& strm,  // NOLINT
                         const CFolderContent& cfc) {
    for (auto it = cfc.cbegin(); it != cfc.cend() ; ++it) {
        const std::shared_ptr<const CFile> p_file = it->second;
        strm << it->first << " = " << *p_file << std::endl;
    }
    return strm;
}

}  // namespace pcs_api

