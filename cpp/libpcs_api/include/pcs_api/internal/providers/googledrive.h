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

#ifndef INCLUDE_PCS_API_INTERNAL_PROVIDERS_GOOGLEDRIVE_H_
#define INCLUDE_PCS_API_INTERNAL_PROVIDERS_GOOGLEDRIVE_H_

#include <string>
#include <vector>

#include "pcs_api/storage_builder.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/oauth2_storage_provider.h"

namespace pcs_api {

/**
 * \brief Google Drive storage provider implementation.
 */
class GoogleDrive : public OAuth2StorageProvider {
 public:
    static const char* kProviderName;
    std::string GetUserId() override;
    CQuota GetQuota() override;
    std::shared_ptr<CFolderContent> ListRootFolder() override;
    std::shared_ptr<CFolderContent> ListFolder(const CPath& path) override;
    std::shared_ptr<CFolderContent> ListFolder(const CFolder& folder) override;
    bool CreateFolder(const CPath& path) override;
    bool Delete(const CPath& path) override;
    std::shared_ptr<CFile> GetFile(const CPath& path) override;
    void Download(const CDownloadRequest& downloadRequest) override;
    void Upload(const CUploadRequest& uploadRequest) override;

 private:
    static StorageBuilder::create_provider_func GetCreateInstanceFunction();
    explicit GoogleDrive(const StorageBuilder& storage_builder);
    void ThrowCStorageException(CResponse *p_response,
                                const CPath* p_opt_path);
    void ValidateGoogleDriveResponse(CResponse *p_response,
                                     const CPath* p_opt_path);
    void ValidateGoogleDriveApiResponse(CResponse *p_response,
                                        const CPath* p_opt_path);
    RequestInvoker GetRequestInvoker(const CPath* p_path);
    RequestInvoker GetApiRequestInvoker(const CPath* p_path = nullptr);
    /**
    * \brief Utility class used to convert a CPath
    *        to a list of google drive files ids.
    *
    * path_ : the path (exists or not)
    * segments_ : vector of segments in c_path (empty for root path)
    * files_chain_ : vector of files json objects.
    * If remote path_ exists, len(files_chain_) = len(segments_).
    * If some trailing files do not exist, files_chain list is shorter,
    * and may even be empty.
    *
    * Examples : a,b are folders, c.pdf is a blob.
    * /a/b/c.pdf --> segments_ = ('a','b','c.pdf')
    *                files_chain_ = [ {'id':'id_a'...},
    *                                 {'id':'id_b'...},
    *                                 {'id':'id_c'...} ]
    *                Exists() ? true
    *                LastIsBlob() ? True (c.pdf is not a folder)
    *
    * /a/b/c.pdf/d --> segments_ = ('a','b','c.pdf', 'd')
    *                  files_chain_ = [ {'id':'id_a'...},
    *                                   {'id':'id_b'...},
    *                                   {'id':'id_c'...} ]
    *                  Exists() ? false
    *                  LastIsBlob() ? true (last is c.pdf)
    *
    * In case c.pdf does not exist :
    * /a/b/c.pdf --> segments_ = ('a','b','c.pdf')
    *                files_chain_ = [ {'id':'id_a'...},
    *                                 {'id':'id_b'...} ]
    *                 Exists() ? false
    *                 LastIsBlob() ? false (if b is folder)
    */
    class RemotePath {
     public:
        RemotePath(const CPath& path,
                   std::vector<web::json::value> files_chain);
        const std::vector<string_t>& segments() const {
            return segments_;
        }

        const std::vector<web::json::value>& files_chain() const {
            return files_chain_;
        }

        /**
        * Does this path exist google side ?
        */
        bool Exists() const;
        /**
        * If this remote path does not exist,
        * this is the last existing id, or 'root'.
        *
        * @return id of deepest folder in filesChain,
        *         or "root" if filesChain is empty
        */
        string_t GetDeepestFolderId() const;

        /**
        * Return deepest object (should be a blob).
        *
        * Object attributes are 'id', 'downloadUrl'
        *
        * @return deepest object
        */
        const web::json::value& GetBlob() const;
        bool LastIsBlob() const;

        /**
        * @param depth
        * @return CPath composed of 'depth' first segments (/ if depth = 0)
        */
        CPath GetFirstSegmentsPath(size_t depth);

        /**
        * @return CPath of last existing file
        */
        CPath LastCPath();

     private:
        const CPath path_;
        const std::vector<string_t> segments_;
        const std::vector<web::json::value> files_chain_;
    };
    const GoogleDrive::RemotePath FindRemotePath(const CPath& path,
                                                 bool detailed);
    std::shared_ptr<CFile> ParseCFile(const CPath& parent_path,
                                      const web::json::value& json);
    string_t RawCreateFolder(const CPath& path, string_t parent_id);
    void DeleteById(const CPath& path, string_t file_id);

    static std::shared_ptr<IStorageProvider> CreateInstance(
                                                const StorageBuilder& builder);
    friend class StorageFacade;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_PROVIDERS_GOOGLEDRIVE_H_


