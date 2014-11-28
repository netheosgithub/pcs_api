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

#include <memory>

#include "pcs_api/user_credentials.h"

namespace pcs_api {


UserCredentials::UserCredentials(const AppInfo& app_info,
                                 const std::string& user_id,
                                 const Credentials& credentials) :
        app_info_(app_info),
        user_id_(user_id),
        p_credentials_(std::unique_ptr<Credentials>(credentials.Clone())) {
}

}  // namespace pcs_api
