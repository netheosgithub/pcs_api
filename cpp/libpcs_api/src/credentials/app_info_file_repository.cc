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

#include <cstdio>
#include <fstream>  // NOLINT(readability/streams)
#include <string>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <map>

#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/fstream.hpp"
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/predicate.hpp"
#include "cpprest/json.h"

#include "pcs_api/app_info_file_repository.h"
#include "pcs_api/oauth2_app_info.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"


namespace pcs_api {

AppInfoFileRepository::AppInfoFileRepository(
                                        const boost::filesystem::path& path)
    : path_(path) {
    ReadAppInfoFile();
}

static std::string GetAppKeyInMap(const std::string& provider_name,
                                  const std::string& app_name) {
    return provider_name + "." + app_name;
}

void AppInfoFileRepository::ReadAppInfoFile() {
    LOG_DEBUG << "Will read AppInfoFile: " << path_;

    boost::filesystem::ifstream infofile(path_);
    if (infofile.fail()) {
        std::system_error se(errno, std::system_category());
        const char *p_msg = se.what();
        BOOST_THROW_EXCEPTION(
            std::ios_base::failure(std::string("Could not open file: ")
                + utility::conversions::to_utf8string(path_.c_str())
                + ": " + p_msg));
    }
    for (std::string line; std::getline(infofile, line) ;) {
        // LOG_TRACE << "reading line =" << line;
        boost::algorithm::trim(line);

        if (line.length() == 0 || line[0] == '#') {
            continue;
        }
        // expected line format is key=value:
        std::string::size_type ieq = line.find_first_of('=');
        if (ieq == std::string::npos) {
            LOG_WARN << "Ignored line: not found '='";
            continue;
        }

        std::string prov_app_name = line.substr(0, ieq);
        boost::algorithm::trim(prov_app_name);
        std::string app_info_value = line.substr(ieq+1);
        boost::algorithm::trim(app_info_value);
        // LOG_TRACE << "prov_app=" << prov_app_name;
        // LOG_TRACE << "app_info=" << app_info_value;

        // split Key: provider_name.app_name
        ieq = prov_app_name.find_first_of('.');
        if (ieq == std::string::npos) {
            LOG_WARN << "Ignored line: not found '.'"
                        " between provider and app names";
            continue;
        }
        std::string provider_name = prov_app_name.substr(0, ieq);
        std::string app_name = prov_app_name.substr(ieq+1);

        // default app_info if login password provider:

        app_info_map_[GetAppKeyInMap(provider_name, app_name)] =
                        BuildAppInfo(provider_name, app_name, app_info_value);
    }
}

const AppInfo& AppInfoFileRepository::GetAppInfo(
                                        const std::string& provider_name,
                                        const std::string& app_name) const {
    if (!app_name.empty()) {
        std::string key = GetAppKeyInMap(provider_name, app_name);
        try {
            return *app_info_map_.at(key);
        } catch (std::out_of_range&) {
            BOOST_THROW_EXCEPTION(
                std::invalid_argument("No application found"
                                      " for provider: " + provider_name
                                      + " and name: " + app_name));
        }
    } else {
        // Caller did not specify any app name,
        // so we'll check if we have only one:
        AppInfo *p_app_info = nullptr;
        for (auto it = app_info_map_.begin();
             it != app_info_map_.end();
             ++it) {
            std::string key = it->first;
            if (boost::algorithm::starts_with(key, provider_name + ".")) {
                if (p_app_info != nullptr) {
                    BOOST_THROW_EXCEPTION(
                        std::invalid_argument("Several applications found"
                                        " for provider: " + provider_name));
                }
                p_app_info = it->second.get();
            }
        }
        if (p_app_info == nullptr) {
            BOOST_THROW_EXCEPTION(std::invalid_argument(
                    "No application found for provider: " + provider_name));
        }
        return *p_app_info;
    }
}

std::unique_ptr<AppInfo> AppInfoFileRepository::BuildAppInfo(
                                    const std::string& provider_name,
                                    const std::string& app_name,
                                    const std::string& app_info_value) const {
    std::unique_ptr<AppInfo> p_app_info;
    web::json::value json_value = web::json::value::parse(
                            utility::conversions::to_string_t(app_info_value));
    web::json::object& json_obj = json_value.as_object();
    auto foundapp = json_obj.find(PCS_API_STRING_T("appId"));
    if (foundapp != json_obj.end()) {
        // It is an OAuth application:
        string_t app_id = foundapp->second.as_string();
        std::vector<std::string> permissions;
        auto foundscope = json_obj.find(PCS_API_STRING_T("scope"));
        if (foundscope != json_obj.end()) {
            const web::json::array& json_permissions =
                                                foundscope->second.as_array();
            // copy to vector (also array will be destroyed):
            for (size_t i = 0 ; i < json_permissions.size() ; ++i) {
                permissions.push_back(utility::conversions::to_utf8string(
                                        json_permissions.at(i).as_string()));
            }
        }
        string_t app_secret =
                        json_obj.at(PCS_API_STRING_T("appSecret")).as_string();
        auto foundurl = json_obj.find(PCS_API_STRING_T("redirectUrl"));
        string_t redirect_url;
        if (foundurl != json_obj.end()) {
            redirect_url = foundurl->second.as_string();
        }
        p_app_info.reset(
            new OAuth2AppInfo(provider_name, app_name,
                            utility::conversions::to_utf8string(app_id),
                            utility::conversions::to_utf8string(app_secret),
                            permissions,
                            utility::conversions::to_utf8string(redirect_url)));
    } else {
        // It is a login/password application:
        p_app_info.reset(new AppInfo(provider_name, app_name));
    }
    LOG_DEBUG << "Built " << p_app_info->ToString();
    return std::move(p_app_info);
}

AppInfoFileRepository::~AppInfoFileRepository() {
}


}  // namespace pcs_api

