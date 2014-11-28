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

#ifndef INCLUDE_PCS_API_STORAGE_BUILDER_H_
#define INCLUDE_PCS_API_STORAGE_BUILDER_H_

#include <string>
#include <map>
#include <vector>

#include "pcs_api/i_storage_provider.h"
#include "pcs_api/app_info_repository.h"
#include "pcs_api/user_credentials_repository.h"

// Avoid including cpprest SDK 'http_client.h':
namespace web {
namespace http {
namespace client {
    class http_client_config;
}  // namespace client
}  // namespace http
}  // namespace web

namespace pcs_api {

class StorageFacade;

/**
 * \brief A class containing informations for building a IStorageProvider.
 */
class StorageBuilder {
 public:
    /**
     * \brief Type of function that creates a storage provider,
     *        given a StorageBuilder.
     */
    typedef std::function<std::shared_ptr<IStorageProvider>(StorageBuilder&)>
                                                          create_provider_func;

    const std::string& provider_name() const {
        return provider_name_;
    }

    /**
     * \brief set application information repository.
     * 
     * @param app_info_repo
     * @param app_name The name of application (may be empty if only one app
     *        in repo for provider)
     */
    StorageBuilder& app_info_repository(
            std::shared_ptr<AppInfoRepository> app_info_repo,
            const std::string& app_name);

    /**
     * \brief Set the user credentials repository.
     *
     * @param user_credentials_repo The repository
     * @param user_id The user identifier (may be empty if the identifier
     *                 is unknown yet)
     * @return This builder
     */
    StorageBuilder& user_credentials_repository(
            std::shared_ptr<UserCredentialsRepository> user_credentials_repo,
            const std::string& user_id);

    /**
     * \brief Set storage to be instantiated without defined user_id
     *
     * OAuth bootstrap is not obvious: storage must be instantiated _without_
     * users credentials (for retrieving userId thanks to accessToken).
     * As this use case is unlikely (instantiating a storage without user
     * credentials), this method indicates the specificity: no 'missing users
     * credentials' error will be raised.
     *
     * @param forBootstrapping true if it is the first start of the API
     * @return The builder
     */
    StorageBuilder& for_bootstrapping(bool for_bootstrapping);

    /**
     * \brief Set retry strategy
     *
     * A default retry strategy is instantiated if this method is not called.
     *
     * @param retry_strategy
     * @return this builder
     */
    StorageBuilder& retry_strategy(
                            std::shared_ptr<RetryStrategy> p_retry_strategy);

    /**
     * \brief Instantiate storage provider implementation.
     *
     * Builds a provider-specific storage implementation, by passing this
     * builder in constructor. Each implementation gets its required
     * information from builder.
     *
     * @return The storage provider instance (no reference is kept by the library, ie use count is 1).
     */
    std::shared_ptr<IStorageProvider> Build();

    std::shared_ptr<pcs_api::UserCredentialsRepository>
                                        user_credentials_repository() const {
        return p_user_credentials_repo_;
    }

    std::shared_ptr<RetryStrategy> retry_strategy() const {
        return p_retry_strategy_;
    }

    const AppInfo& GetAppInfo() const;

    /**
    * \brief Get the detailed http configuration to be used for connections.
    *
    * Can be used to set a proxy, timeouts, etc. http_client_config type is
    * defined in cpprest SDK / file http_client.h
    * Refer to cpprestsdk documentation for details.
    * Note that http_config class default timeout value of 30 seconds
    * is not enough.
    * StorageBuilder has a default configuration with a suitable value.
    *
    * @return http client configuration (modifiable).
    */
    std::shared_ptr<web::http::client::http_client_config>
                                                    http_client_config() const;

    /**
     * Can be called only after Build() has been called.
     *
     * @return user credentials, or empty shared_ptr for bootstrapping
     */
    std::shared_ptr<UserCredentials> GetUserCredentials() const;

 private:
    std::string provider_name_;
    create_provider_func create_instance_func_;
    std::shared_ptr<pcs_api::AppInfoRepository> p_app_info_repo_;
    std::string app_name_;
    std::shared_ptr<pcs_api::UserCredentialsRepository>
                                                    p_user_credentials_repo_;
    std::shared_ptr<pcs_api::UserCredentials> p_user_credentials_;
    std::string user_id_;
    bool for_bootstrapping_;
    // because this type may be unknown to pcs_api user,
    // we have to use a pointer:
    std::shared_ptr<web::http::client::http_client_config>
                                                    p_http_client_config_;
    std::shared_ptr<pcs_api::RetryStrategy> p_retry_strategy_;

    StorageBuilder(const std::string& provider_name,
                   create_provider_func create_instance);
    friend class StorageFacade;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_STORAGE_BUILDER_H_

