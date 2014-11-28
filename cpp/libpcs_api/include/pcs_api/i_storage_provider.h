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

#ifndef INCLUDE_PCS_API_I_STORAGE_PROVIDER_H_
#define INCLUDE_PCS_API_I_STORAGE_PROVIDER_H_

#include <string>
#include <map>
#include <vector>

#include "pcs_api/model.h"
#include "pcs_api/credentials.h"

namespace pcs_api {


/**
 * \brief Common interface for storage providers.
 */
class IStorageProvider {
 public:
    /**
     * \brief Get the provider name.
     *
     * @return The provider name (ie. "dropbox")
     */
    virtual std::string GetProviderName() const = 0;

    /**
     * \brief Get user identifier.
     *
     * @return user identifier (login in case of login/password,
     *         or email in case of OAuth.
     */
    virtual std::string GetUserId() = 0;

    /**
     * \brief Inquire about space usage.
     *
     * @return a CQuota object
     */
    virtual CQuota GetQuota() = 0;

    /**
     * \brief Equivalent to ListFolder(CPath) with root path "/"
     *
     * @return TODO
     * @throws CStorageException Error getting the root folder
     */
    virtual std::shared_ptr<CFolderContent> ListRootFolder() = 0;

    /**
     * \brief List files in folder at given path.
     *
     * Throws CInvalidFileTypeException if given path is a blob.
     * Note: objects in returned map may have incomplete information
     *
     * @param path The folder path
     * @return a map of files present at given CPath. keys of map are CPath
     *         objects, values are CFile objects (CFolder or CBlob),
     *         or empty shared pointer if no folder exists at given path.
     * @throws CStorageException Error getting the files in the folder
     */
    virtual std::shared_ptr<CFolderContent> ListFolder(const CPath& path) = 0;

    /**
     * \brief List files in folder at given path.
     *
     * Throws CInvalidFileTypeException if given path is a blob.
     * Note: objects in returned map may have incomplete information
     *
     * @param path The folder path
     * @return a map of files present at given CPath. keys of map are CPath
     *         objects, values are CFile objects (CFolder or CBlob),
     *         or empty shared pointer if no folder exists at given path.
     * @throws CStorageException Error getting the files in the folder
     */
    virtual std::shared_ptr<CFolderContent> ListFolder(
                                                    const CFolder& folder) = 0;

    /**
     * \brief Create a folder at given path, with intermediate folders
     *        if needed.
     *
     * Throws CInvalidFileType exception if a blob exists at this path.
     *
     * @param path The folder path to create
     * @return true if folder has been created,
     *         false if it was already existing.
     * @throws CStorageException Error creating the folder
     */
    virtual bool CreateFolder(const CPath& path) = 0;

    /**
     * \brief Deletes blob, or recursively delete folder at given path.
     *
     * @param path The file path to delete
     * @return true if at least one file was deleted,
     *         false if no object was existing at this path.
     * @throws CStorageException Error deleting the file
     */
    virtual bool Delete(const CPath& path) = 0;

    /**
     * \brief Get detailed file information at given path.
     *
     * @param path The file path
     * @return detailed file information at given path, or empty shared pointer
     *         if no object exists at this path
     * @throws CStorageException Error getting the file
     */
    virtual std::shared_ptr<CFile> GetFile(const CPath& path) = 0;

    /**
     * \brief Downloads a blob from provider to a byte sink.
     * 
     * Throws CFileNotFoundError if no blob exists at this path.
     * Throws CInvalidFileTypeError if a folder exists at specified path.
     *
     * @param downloadRequest The download request object (defines path,
     *                       byte sink...)
     * @throws CStorageException Download error
     */
    virtual void Download(const CDownloadRequest& downloadRequest) = 0;

    /**
     * \brief Uploads data to provider from a byte source.
     * 
     * If blob already exists it is replaced.
     * Throws CInvalidFileTypeError if a folder already exists
     * at specified path.
     *
     * @param uploadRequest The upload request object
     * @throws CStorageException Upload error
     */
    virtual void Upload(const CUploadRequest& uploadRequest) = 0;

    /**
     * \brief base destructor is virtual.
     */
    virtual ~IStorageProvider() {
    }
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_I_STORAGE_PROVIDER_H_

