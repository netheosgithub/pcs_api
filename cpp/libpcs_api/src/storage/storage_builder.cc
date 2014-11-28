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

#include <mutex>

#include "cpprest/http_client.h"

#include "pcs_api/storage_builder.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

/**
 * Default timeout.
 */
static const int kDefaultTimeout_s = 3 * 60;


StorageBuilder::StorageBuilder(const std::string& provider_name,
                               create_provider_func create_instance)
    : provider_name_(provider_name),
      create_instance_func_(create_instance),
      p_retry_strategy_(std::make_shared<RetryStrategy>(5, 1000)),
      for_bootstrapping_(false) {
    // Create now a default http_client_config:
    p_http_client_config_.reset(new web::http::client::http_client_config());
    p_http_client_config_->set_timeout(utility::seconds(kDefaultTimeout_s));
}

StorageBuilder& StorageBuilder::app_info_repository(
                    std::shared_ptr<pcs_api::AppInfoRepository> app_info_repo,
                    const std::string& app_name) {
    p_app_info_repo_ = app_info_repo;
    app_name_ = app_name;
    return *this;
}

StorageBuilder& StorageBuilder::user_credentials_repository(
        std::shared_ptr<pcs_api::UserCredentialsRepository>
                                                        p_user_credentials_repo,
        const std::string& user_id) {
    p_user_credentials_repo_ = p_user_credentials_repo;
    user_id_ = user_id;
    return *this;
}

StorageBuilder& StorageBuilder::for_bootstrapping(bool for_bootstrapping) {
    for_bootstrapping_ = for_bootstrapping;
    return *this;
}

StorageBuilder& StorageBuilder::retry_strategy(
                    std::shared_ptr<pcs_api::RetryStrategy> p_retry_strategy) {
    p_retry_strategy_ = p_retry_strategy;
    return *this;
}

std::shared_ptr<IStorageProvider> StorageBuilder::Build() {
    if (!p_app_info_repo_) {
        BOOST_THROW_EXCEPTION(
            std::logic_error("Undefined application information repository"));
    }
    if (!p_user_credentials_repo_) {
        BOOST_THROW_EXCEPTION(
            std::logic_error("Undefined user credentials repository"));
    }

    const AppInfo& app_info = p_app_info_repo_->GetAppInfo(provider_name_,
                                                           app_name_);
    if (!for_bootstrapping_) {
        // Usual case: retrieve user credentials
        // Note that user_id_ may be undefined, repository should handle this:
        p_user_credentials_ = p_user_credentials_repo_->Get(app_info, user_id_);
    } else {
        // Special case for getting initial oauth tokens:
        // we'll instantiate without any user_credentials
    }

    return create_instance_func_(*this);
}

std::shared_ptr<web::http::client::http_client_config>
StorageBuilder::http_client_config() const {
    return p_http_client_config_;
}

const AppInfo& StorageBuilder::GetAppInfo() const {
    return p_app_info_repo_->GetAppInfo(provider_name_, app_name_);
}

std::shared_ptr<UserCredentials> StorageBuilder::GetUserCredentials() const {
    return p_user_credentials_;
}



}  // namespace pcs_api
