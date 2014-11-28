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

#ifndef INCLUDE_PCS_API_INTERNAL_PROVIDERS_HUBIC_H_
#define INCLUDE_PCS_API_INTERNAL_PROVIDERS_HUBIC_H_

#include <string>
#include <mutex>

#include "pcs_api/storage_builder.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/oauth2_storage_provider.h"

namespace pcs_api {

class SwiftClient;

/**
 * \brief hubiC storage provider implementation.
 */
class Hubic : public OAuth2StorageProvider {
 public:
    static const char* kProviderName;
    std::string GetUserId() override;
    CQuota GetQuota() override;
    std::shared_ptr<CFolderContent> ListRootFolder() override;
    std::shared_ptr<CFolderContent> ListFolder(const CFolder& folder) override;
    std::shared_ptr<CFolderContent> ListFolder(const CPath& path) override;
    bool CreateFolder(const CPath& path) override;
    bool Delete(const CPath& path) override;
    std::shared_ptr<CFile> GetFile(const CPath& path) override;
    void Download(const CDownloadRequest& download_request) override;
    void Upload(const CUploadRequest& upload_request) override;

 private:
    std::mutex swift_client_mutex_;
    std::shared_ptr<SwiftClient> p_swift_client_;

    explicit Hubic(const StorageBuilder& storage_builder);
    void ThrowCStorageException(CResponse *p_response,
                                const CPath* p_opt_path,
                                bool throw_retriable = false);
    void ValidateHubicApiResponse(CResponse *p_response,
                                  const CPath* p_opt_path);
    RequestInvoker GetApiRequestInvoker(const CPath* p_opt_path = nullptr);
    std::shared_ptr<SwiftClient> GetSwiftClient();
    void SwiftCall(std::function<void(SwiftClient *p_swift)> user_func);
    //
    static StorageBuilder::create_provider_func GetCreateInstanceFunction();
    static std::shared_ptr<IStorageProvider> CreateInstance(
                                                const StorageBuilder& builder);
    friend class StorageFacade;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_PROVIDERS_HUBIC_H_


