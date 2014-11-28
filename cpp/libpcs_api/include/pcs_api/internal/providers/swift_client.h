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

#ifndef INCLUDE_PCS_API_INTERNAL_PROVIDERS_SWIFT_CLIENT_H_
#define INCLUDE_PCS_API_INTERNAL_PROVIDERS_SWIFT_CLIENT_H_

#include <string>
#include <vector>

#include "cpprest/http_msg.h"

#include "pcs_api/model.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/retry_strategy.h"
#include "pcs_api/internal/request_invoker.h"
#include "pcs_api/internal/c_response.h"

namespace pcs_api {

/**
 * \brief An implementation of Openstask Swift client
 *        (used by hubiC provider impl).
 *
 * All swift files are "objects", all objects have an absolute name
 * (no folder hierarchy, slash in pathname is not a separator but a standard
 * character).
 * hubiC needs empty directory objects with content-type "application/directory"
 * to materialize folders: these are called directory markers.
 * But swift can also work without these empty objects, hence the flag in
 * constructor.
 *
 * This class aims to be general, in case several providers use Swift back-end.
 *
 * See http://docs.openstack.org/api/openstack-object-storage/1.0/content/
 * for reference.
 */
class SwiftClient {
 public:
    SwiftClient(const string_t& account_endpoint,
                const string_t& auth_token,
                std::unique_ptr<RetryStrategy> p_retry_strategy,
                bool use_directory_markers,
                std::function<std::shared_ptr<CResponse>(
                    web::http::http_request request)> execute_request_function);
    void UseFirstContainer();
    std::shared_ptr<CFolderContent> ListFolder(const CPath& path);
    bool CreateFolder(const CPath& path);
    bool Delete(const CPath& path);
    std::shared_ptr<CFile> GetFile(const CPath& path);
    void Download(const CDownloadRequest& download_request);
    void Upload(const CUploadRequest& upload_request);

 private:
    const string_t account_endpoint_;
    const string_t auth_token_;
    std::unique_ptr<RetryStrategy> p_retry_strategy_;
    const bool use_directory_markers_;
    const std::function<std::shared_ptr<CResponse>(
            web::http::http_request request)> execute_request_function_;
    string_t current_container_;

    /**
     * \brief add authorization token to request headers
     */
    void ConfigureRequest(web::http::http_request *p_request,
                          const string_t& format);
    /**
     * \brief add authorization token to request headers,
     *        and execute request thanks to execute_request_function_().
     */
    std::shared_ptr<CResponse> ConfigureAndExecuteRequest(
            web::http::http_request request, string_t format);
    void ValidateSwiftResponse(CResponse *p_response, const CPath* p_opt_path);
    void ValidateSwiftApiResponse(CResponse *p_response,
                                  const CPath* p_opt_path);
    RequestInvoker GetBasicRequestInvoker(const CPath& p_path);
    RequestInvoker GetApiRequestInvoker(const CPath* p_opt_path = nullptr);
    /**
     * Quick object check: do a HEAD and return headers
     * (or empty pointer if no remote object)
     */
    std::unique_ptr<web::http::http_headers> HeadOrNull(const CPath& path);
    void UseContainer(string_t container_name);
    std::vector<string_t> GetContainers();
    /**
     * \brief Create a folder without creating any higher
     *        level intermediate folders.
     *
     * @param path The folder path
     */
    void RawCreateFolder(const CPath& path);
    /**
     * \brief Create any parent folders if they do not exist, to meet old
     *        swift convention.
     *
     * hubiC requires these objects for the sub-objects to be visible in webapp.
     * As an optimization, we consider that if folder a/b/c exists, then a/
     * and a/b/ also exist so are not checked nor created.
     *
     * @param leaf_folder_path
     */
    void CreateIntermediateFoldersObjects(const CPath& leaf_folder_path);
    web::json::value ListObjectsWithinFolder(const CPath& path,
                                             string_t delimiter);
    string_t GetObjectUrl(const CPath& path);
    string_t GetCurrentContainerUrl();
};

namespace swift_details {
    boost::posix_time::ptime ParseLastModified(const web::json::value& val);
    boost::posix_time::ptime ParseTimestamp(
                                       const web::http::http_headers& headers);
}  // namespace swift_details

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_PROVIDERS_SWIFT_CLIENT_H_


