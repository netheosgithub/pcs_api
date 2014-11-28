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

#include "cpprest/http_client.h"

#include "pcs_api/internal/http_client_pool.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

HttpClientPool::HttpClientPool(const web::uri& base_uri,
        std::shared_ptr<web::http::client::http_client_config> client_config)
    : ObjectPool(std::bind(&HttpClientPool::CreateClient,
                           this),
                 std::bind(&HttpClientPool::DeleteClient,
                           this,
                           std::placeholders::_1)),
      base_uri_(base_uri),
      p_http_client_config_(client_config) {
}

web::http::client::http_client* HttpClientPool::CreateClient() {
    return new web::http::client::http_client(base_uri_,
                                              *p_http_client_config_);
}

void HttpClientPool::DeleteClient(web::http::client::http_client *p_client) {
    delete p_client;
}

}  // namespace pcs_api
