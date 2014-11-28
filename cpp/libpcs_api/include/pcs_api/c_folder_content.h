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

#ifndef INCLUDE_PCS_API_C_FOLDER_CONTENT_H_
#define INCLUDE_PCS_API_C_FOLDER_CONTENT_H_

#include <string>
#include <memory>
#include <map>

#include "pcs_api/c_file.h"


namespace pcs_api {

class CFolderContentBuilder;

/**
 * \brief Map-like object storing the content of a remote folder.
 */
class CFolderContent {
 public:
    typedef std::map<CPath, std::shared_ptr<CFile>> content_map;

    const content_map::const_iterator cbegin() const;
    const content_map::const_iterator cend() const;

    /**
     * \brief check if given path exists in folder
     * @param path
     * @return true if a file with that path exists
     */
    bool ContainsPath(const CPath& path) const;

    /**
     * \brief Get a file by its path.
     *
     * @param path The file path to search
     * @return The file found or null if the path is not in the folder
     */
    std::shared_ptr<CFile> GetFile(const CPath& path) const;

    /**
     * \brief Inquire if folder is empty.
     *
     * @return true if the folder is empty, false otherwise
     */
    bool empty() const {
        return p_content_map_->empty();
    }

    /**
     * \brief Get the files count in the folder.
     *
     * @return The files count
     */
    std::size_t size() const {
        return p_content_map_->size();
    }

 private:
    friend class CFolderContentBuilder;
    explicit CFolderContent(std::unique_ptr<content_map> p_content);
    std::unique_ptr<content_map> p_content_map_;
};

std::ostream& operator<<(std::ostream& strm, const CFolderContent& cfc);


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_FOLDER_CONTENT_H_


