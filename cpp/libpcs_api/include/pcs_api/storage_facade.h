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

#ifndef INCLUDE_PCS_API_STORAGE_FACADE_H_
#define INCLUDE_PCS_API_STORAGE_FACADE_H_

#include <string>
#include <map>
#include <vector>

#include "pcs_api/storage_builder.h"

namespace pcs_api {

/**
 * \brief The class with primary static method for instantiating
 *        an IStorageProvider.
 *
 * Conceptually, this class holds the registry of all providers.
 */
class StorageFacade {
 public:
    StorageFacade();
    static StorageBuilder ForProvider(std::string provider_name);
    static std::vector<std::string> GetRegisteredProviders();
    static void RegisterProvider(
                    const char *p_provider_name,
                    StorageBuilder::create_provider_func create_instance);

 private:
    static void RegisterOnceDefaultProviders();
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_STORAGE_FACADE_H_

