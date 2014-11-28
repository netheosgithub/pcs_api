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

#ifndef INCLUDE_PCS_API_INTERNAL_STORAGE_PROVIDER_H_
#define INCLUDE_PCS_API_INTERNAL_STORAGE_PROVIDER_H_

#include <string>
#include <functional>

#include "cpprest/http_client.h"

#include "pcs_api/internal/c_folder_content_builder.h"
#include "pcs_api/internal/c_response.h"

namespace pcs_api {

/**
 * \brief Base class for all storage providers implementations.
 */
template<class session_manager_T>
class StorageProvider: public IStorageProvider {
 public:
    StorageProvider(const std::string& provider_name,
                    std::shared_ptr<session_manager_T> p_session_manager,
                    std::shared_ptr<RetryStrategy> p_retry_strategy) :
        provider_name_(provider_name),
        p_session_manager_(p_session_manager),
        p_retry_strategy_(p_retry_strategy) {
    }

    std::string GetProviderName() const {
        return provider_name_;
    }

 protected:
    std::shared_ptr<session_manager_T> p_session_manager_;
    const std::shared_ptr<RetryStrategy> p_retry_strategy_;

 private:
    const std::string provider_name_;
    friend class OAuth2Bootstrapper;

    // gtest_prod.h : FRIEND_TEST(BasicTest, TestGetUserId)
    friend class BasicTest_TestGetUserId_Test;
};


}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_INTERNAL_STORAGE_PROVIDER_H_

