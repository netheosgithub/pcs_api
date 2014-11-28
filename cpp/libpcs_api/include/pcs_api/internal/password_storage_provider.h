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

#ifndef INCLUDE_PCS_API_INTERNAL_PASSWORD_STORAGE_PROVIDER_H_
#define INCLUDE_PCS_API_INTERNAL_PASSWORD_STORAGE_PROVIDER_H_

#include <string>
#include <functional>

#include "cpprest/http_client.h"

#include "pcs_api/internal/storage_provider.h"
#include "pcs_api/internal/password_session_manager.h"

namespace pcs_api {

/**
 * \brief A specialization of StorageProvider
 *        that uses an internal PasswordSessionManager.
 */
typedef StorageProvider<PasswordSessionManager> PasswordStorageProvider;

}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_INTERNAL_PASSWORD_STORAGE_PROVIDER_H_

