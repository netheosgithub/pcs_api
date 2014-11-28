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

#ifndef INCLUDE_PCS_API_INTERNAL_PROVIDERS_CLOUDME_H_
#define INCLUDE_PCS_API_INTERNAL_PROVIDERS_CLOUDME_H_

#include <memory>
#include <string>
#include <map>
#include <vector>

#include "boost/property_tree/ptree.hpp"

#include "pcs_api/storage_builder.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/password_storage_provider.h"
#include "pcs_api/internal/request_invoker.h"

namespace pcs_api {

/**
 * \brief CloudMe storage provider implementation.
 *
 * Note: CloudMe folders have no modification time.
 */
class CloudMe : public PasswordStorageProvider {
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
    /**
     * \brief id of root folder; cached for performances
     */
    std::string root_id_;
    static StorageBuilder::create_provider_func GetCreateInstanceFunction();
    explicit CloudMe(const StorageBuilder& storage_builder);
    void ThrowCStorageException(CResponse *p_response,
                                const CPath* p_opt_path);
    void ValidateCloudMeResponse(CResponse *p_response, const CPath* p_path);
    void ValidateCloudMeApiResponse(CResponse *p_response,
                                    const CPath* p_opt_path);
    RequestInvoker GetBasicRequestInvoker(const CPath* p_path);
    RequestInvoker GetApiRequestInvoker(const CPath* p_path = nullptr);
    std::string GetRootId();
    const boost::property_tree::ptree GetLogin();
    const string_t BuildRestUrl(const string_t& method_path) const;

    /**
     * Utility class used to hold a CloudMe folder +
     * (optional) parent + children, with file id.
     * All strings are UTF-8 encoded.
     * Each CMFolder "owns" its children, so destroying the root folder destroys
     * all CMFolder objects.
     */
    class CMFolder {
     public:
         // for root folder only (no name, no parent):
        explicit CMFolder(const std::string& id);
        CMFolder(const CMFolder *p_parent,
                 const std::string& id,
                 const std::string& name);
        CMFolder(const CMFolder&) = default;
        CMFolder& operator=(const CMFolder&) = delete;

        const std::string& id() const {
            return id_;
        }
        const std::string& name() const {
            return name_;
        }
        const std::map<std::string, CMFolder>& children() const {
            return children_;
        }
        const std::shared_ptr<CFolder> ToCFolder() const;
        const CPath GetPath() const;
        CMFolder* AddChild(const std::string& id, const std::string& name);
        /**
         * \brief Find in children for a folder with given path.
         *
         * (this method is intended to be used from root folder only).
         *
         * @return folder found, or nullptr if not found
         */
        CMFolder* GetFolder(const CPath& path);
        /**
         * \brief Find in immediate children for a folder with given base name.
         *
         * @return folder found, or nullptr if not found
         */
        CMFolder* GetChildByName(const std::string& name);

     private:
        const CMFolder *p_parent_;
        const std::string id_;
        const std::string name_;
        std::map<std::string, CMFolder> children_;
    };

    /**
     * Utility class used to hold a CloudMe blob, with file id.
     * All strings are UTF-8 encoded.
     */
    class CMBlob {
     public:
        static std::unique_ptr<CMBlob> BuildFromXmlElement(
                                const CMFolder& parent,
                                const boost::property_tree::ptree& element);
        CMBlob(const CMFolder& folder,
                const std::string& id,
                const std::string& name,
                int64_t length,
                const ::boost::posix_time::ptime& updated,
                const std::string& content_type);
        const std::string& id() const {
            return id_;
        }
        const std::string& name() const {
            return name_;
        }
        const CPath GetPath() const;
        const std::shared_ptr<CBlob> ToCBlob() const;

     private:
        const std::string id_;
        const CMFolder& folder_;
        const std::string name_;
        const int64_t length_;
        const ::boost::posix_time::ptime updated_;
        const std::string content_type_;
    };
    std::unique_ptr<CMFolder> LoadFoldersStructure();
    void ScanFolderLevel(const boost::property_tree::ptree& element,
                         CloudMe::CMFolder *p_cm_folder);
    std::unique_ptr<CMBlob> GetBlobByName(const CMFolder* p_cm_folder,
                                          const std::string& base_name);
    std::vector<std::unique_ptr<CMBlob>> ListBlobs(const CMFolder& cm_folder);
    /**
     * Creates folder with given path, with required intermediate folders.
     *
     * @param p_cm_root contains the whole folders structure
     * @param cpath path of folder to create
     * @return the created folder corresponding to targeted cpath
     * @throws CInvalidFileTypeException if a blob exists along that path
     */
    CMFolder* CreateIntermediateFolders(CloudMe::CMFolder* p_cm_root,
                                        const CPath& cpath);
    CMFolder* RawCreateFolder(CMFolder *p_cm_parent_folder,
                              const std::string& base_name);
    static std::shared_ptr<IStorageProvider> CreateInstance(
                                                const StorageBuilder& builder);
    friend class StorageFacade;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_PROVIDERS_CLOUDME_H_


