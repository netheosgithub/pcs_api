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

#ifndef INCLUDE_PCS_API_APP_INFO_FILE_REPOSITORY_H_
#define INCLUDE_PCS_API_APP_INFO_FILE_REPOSITORY_H_

#include <string>
#include <memory>
#include <map>
#include <mutex>

#include "boost/filesystem.hpp"

#include "pcs_api/app_info_repository.h"

namespace pcs_api {

/**
 * \brief Implementation of AppInfoRepository interface based on a flat file.
 *
 * This class is provided only for development purposes, user of libpcs_api
 * is in charge of a more robust and scalable implementation.
 */
class AppInfoFileRepository : public AppInfoRepository {
 public:
    explicit AppInfoFileRepository(const boost::filesystem::path& path);
    const AppInfo& GetAppInfo(const std::string& provider_name,
                              const std::string& app_name) const override;
    ~AppInfoFileRepository();

 private:
    boost::filesystem::path path_;
    std::map<std::string, std::unique_ptr<AppInfo>> app_info_map_;
    void ReadAppInfoFile();
    std::unique_ptr<AppInfo> BuildAppInfo(const std::string& provider_name,
        const std::string& app_name,
        const std::string& json_str) const;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_APP_INFO_FILE_REPOSITORY_H_

