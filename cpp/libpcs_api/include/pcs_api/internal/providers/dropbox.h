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

#ifndef INCLUDE_PCS_API_INTERNAL_PROVIDERS_DROPBOX_H_
#define INCLUDE_PCS_API_INTERNAL_PROVIDERS_DROPBOX_H_

#include <string>

#include "pcs_api/storage_builder.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/oauth2_storage_provider.h"

namespace pcs_api {

/**
 * \brief Dropbox storage provider implementation.
 */
class Dropbox : public OAuth2StorageProvider {
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
     * should be "dropbox" or "sandbox", see
     * https://www.dropbox.com/developers/reference/devguide#app-permissions
     * Used for building URL
     */
    string_t scope_;
    static StorageBuilder::create_provider_func GetCreateInstanceFunction();
    explicit Dropbox(const StorageBuilder& storage_builder);
    void ThrowCStorageException(CResponse *p_response,
                                std::string msg,
                                const CPath* p_opt_path,
                                bool throwRetriable = false);
    void ValidateDropboxResponse(CResponse *p_response,
                                 const CPath* p_opt_path);
    void ValidateDropboxApiResponse(CResponse *p_response,
                                    const CPath* p_opt_path);
    RequestInvoker GetApiRequestInvoker(const CPath* p_opt_path = nullptr);
    RequestInvoker GetRequestInvoker(const CPath* p_path);
    const web::json::value GetAccount();
    string_t BuildUrl(const string_t& root, const string_t& method_path);
    void AddPathToUrl(string_t* p_url, const CPath& path);
    string_t BuildApiUrl(const string_t& method_path);
    string_t BuildFileUrl(const string_t& method_path, const CPath& path);
    string_t BuildContentUrl(string_t method_path, CPath path);
    std::shared_ptr<CFile> ParseCFile(const web::json::object& file_obj);
    //
    static std::shared_ptr<IStorageProvider> CreateInstance(
                                        const StorageBuilder& builder);
    friend class StorageFacade;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_PROVIDERS_DROPBOX_H_


