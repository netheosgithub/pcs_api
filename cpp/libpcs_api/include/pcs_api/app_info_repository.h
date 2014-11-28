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

#ifndef INCLUDE_PCS_API_APP_INFO_REPOSITORY_H_
#define INCLUDE_PCS_API_APP_INFO_REPOSITORY_H_

#include <string>
#include <memory>
#include <vector>

#include "pcs_api/types.h"
#include "pcs_api/app_info.h"

namespace pcs_api {

/**
* \brief Interface used by pcs_api to get applications information.
*/
class AppInfoRepository {
 public:
    /**
     * \brief Retrieve application information.
     * 
     * @param provider_name
     * @param app_name may be empty if a single application exists
     *        for this provider.
     * @return a const reference to application information.
     *         Valid till repository exists.
     */
    virtual const AppInfo& GetAppInfo(const std::string& provider_name,
                                      const std::string& app_name) const = 0;
    virtual ~AppInfoRepository() {
    }
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_APP_INFO_REPOSITORY_H_

