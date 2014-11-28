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

#include "pcs_api/storage_facade.h"
#include "pcs_api/internal/logger.h"
// For providers registration only:
#include "pcs_api/internal/providers/cloudme.h"
#include "pcs_api/internal/providers/dropbox.h"
#include "pcs_api/internal/providers/hubic.h"
#include "pcs_api/internal/providers/googledrive.h"

namespace pcs_api {

// These variables are created once, and never destroyed:
// See google style guide, Static_and_Global_Variables
std::mutex *g_PROVIDERS_REGISTRY_LOCK;
std::map<std::string, StorageBuilder::create_provider_func>
                                                        *g_PROVIDERS_REGISTRY;

// The static singleton variable that will be instantiated by compiler:
static StorageFacade registry_initializer_;

StorageFacade::StorageFacade() {
    if (g_PROVIDERS_REGISTRY_LOCK == nullptr) {  // defensive code
        g_PROVIDERS_REGISTRY_LOCK = new std::mutex();
        g_PROVIDERS_REGISTRY =
             new std::map<std::string, StorageBuilder::create_provider_func>();
        StorageFacade::RegisterOnceDefaultProviders();
    }
}

void StorageFacade::RegisterOnceDefaultProviders() {
#if defined _WIN32 || defined _WIN64
    // Digest authentication is only supported on Windows:
    // https://casablanca.codeplex.com/workitem/89
    RegisterProvider(CloudMe::kProviderName,
                     CloudMe::GetCreateInstanceFunction());
#endif
    RegisterProvider(Dropbox::kProviderName,
                     Dropbox::GetCreateInstanceFunction());
    RegisterProvider(Hubic::kProviderName,
                     Hubic::GetCreateInstanceFunction());
    RegisterProvider(GoogleDrive::kProviderName,
                     GoogleDrive::GetCreateInstanceFunction());
    // Register other providers here...
}

StorageBuilder StorageFacade::ForProvider(std::string provider_name) {
    std::lock_guard<std::mutex> lock(*g_PROVIDERS_REGISTRY_LOCK);
    auto it = g_PROVIDERS_REGISTRY->find(provider_name);
    if (it == g_PROVIDERS_REGISTRY->end()) {
        BOOST_THROW_EXCEPTION(std::invalid_argument(
                "No provider implementation registered for name: "
                + provider_name));
    }
    StorageBuilder::create_provider_func create_instance = it->second;
    return StorageBuilder(provider_name, create_instance);
}


std::vector<std::string> StorageFacade::GetRegisteredProviders() {
    std::lock_guard<std::mutex> lock(*g_PROVIDERS_REGISTRY_LOCK);
    std::vector<std::string> ret;
    for (auto it = g_PROVIDERS_REGISTRY->begin();
         it != g_PROVIDERS_REGISTRY->end();
         ++it) {
        ret.push_back(it->first);
    }
    return ret;
}

void StorageFacade::RegisterProvider(const char* p_provider_name,
                        StorageBuilder::create_provider_func create_instance) {
    std::string provider_name(p_provider_name);
    std::lock_guard<std::mutex> lock(*g_PROVIDERS_REGISTRY_LOCK);
    // We allow provider replacement:
    (*g_PROVIDERS_REGISTRY)[provider_name] = create_instance;
}



}  // namespace pcs_api
