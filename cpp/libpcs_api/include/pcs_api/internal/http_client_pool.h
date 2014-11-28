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

#ifndef INCLUDE_PCS_API_INTERNAL_HTTP_CLIENT_POOL_H_
#define INCLUDE_PCS_API_INTERNAL_HTTP_CLIENT_POOL_H_

#include <functional>
#include <list>

#include "cpprest/http_client.h"

#include "pcs_api/internal/object_pool.h"

namespace pcs_api {

/**
 * A class for creating and handling a pool of http_client objects,
 * all sharing the same base uri and configuration.
 *
 * The aim of this class was to avoid duplicate requests in case of digest
 * authentication (CloudMe), hoping that the http_client instance keeps server
 * nonce somewhere (indirectly, in WinHttp handle for win7 implementation).
 * But this does not work, refer to
 * https://casablanca.codeplex.com/discussions/561171
 */
class HttpClientPool : public ObjectPool<web::http::client::http_client> {
 public:
    HttpClientPool(const web::uri& base_uri,
        std::shared_ptr<web::http::client::http_client_config> client_config);
    HttpClientPool(const HttpClientPool&) = delete;
    HttpClientPool& operator=(const HttpClientPool&) = delete;

 private:
    const web::uri base_uri_;
    std::shared_ptr<web::http::client::http_client_config>
                                                         p_http_client_config_;
    web::http::client::http_client* CreateClient();
    void DeleteClient(web::http::client::http_client *);
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_HTTP_CLIENT_POOL_H_
