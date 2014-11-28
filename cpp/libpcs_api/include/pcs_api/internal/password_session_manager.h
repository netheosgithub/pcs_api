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

#ifndef INCLUDE_PCS_API_INTERNAL_PASSWORD_SESSION_MANAGER_H_
#define INCLUDE_PCS_API_INTERNAL_PASSWORD_SESSION_MANAGER_H_

#include <memory>
#include <mutex>

#include "cpprest/http_msg.h"
#include "cpprest/http_client.h"

#include "pcs_api/storage_builder.h"
#include "pcs_api/internal/http_client_pool.h"
#include "pcs_api/internal/c_response.h"

namespace pcs_api {

/**
 * \brief A class for executing basic and digest authenticated http requests.
 */
class PasswordSessionManager {
 public:
    explicit PasswordSessionManager(const StorageBuilder& builder,
                                    const web::uri& base_uri);
    PasswordSessionManager(const PasswordSessionManager&) = delete;
    PasswordSessionManager& operator=(const PasswordSessionManager&) = delete;

    /**
     * \brief Execute the given request, taking a client from the pool.
     *
     * Any server challenge / Authorization header is handled by this method.
     * The client used will be put back in the pool when the returned CResponse
     * object will be destroyed.
     *
     * @param request
     * @return a CResponse object, owning some request info, client,
     *         and http_response.
     */
    std::shared_ptr<CResponse> Execute(::web::http::http_request request);

 private:
    HttpClientPool clients_pool_;
    void ReleaseClient(web::http::client::http_client *p_client);

    // gtest_prod.h : FRIEND_TEST(BasicTest, TestGetUserId)
    friend class BasicTest_TestGetUserId_Test;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_PASSWORD_SESSION_MANAGER_H_

