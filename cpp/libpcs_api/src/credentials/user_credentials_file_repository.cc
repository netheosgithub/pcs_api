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

#include "pcs_api/user_credentials_file_repository.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/logger.h"


namespace pcs_api {


UserCredentialsFileRepository::UserCredentialsFileRepository(
                                        const boost::filesystem::path& path)
    : path_(path) {
    ReadUserCredentialsFile();
}

static std::string GetAppPrefix(const AppInfo& app_info) {
    return utility::conversions::to_utf8string(app_info.provider_name())
            + "."
            + utility::conversions::to_utf8string(app_info.app_name()) + ".";
}

static std::string GetUserKey(const AppInfo& app_info,
                              const std::string& user_id) {
    if (user_id.empty()) {
        BOOST_THROW_EXCEPTION(
            std::invalid_argument("user_id should not be empty"));
    }
    return GetAppPrefix(app_info) + user_id;
}


std::unique_ptr<UserCredentials> UserCredentialsFileRepository::Get(
                                            const AppInfo &app_info,
                                            const std::string& user_id) const {
    std::lock_guard<std::mutex> guard(lock_);

    Credentials *p_credentials = nullptr;
    std::string actual_user_id(user_id);
    if (!user_id.empty()) {
        // Usual case: user_id is defined
        auto key = GetUserKey(app_info, user_id);
        auto it = user_credentials_map_.find(key);
        if (it == user_credentials_map_.end()) {
            BOOST_THROW_EXCEPTION(
                std::invalid_argument(
                    "User " + utility::conversions::to_utf8string(user_id)
                    + " not found for " + app_info.ToString()));
        }
        p_credentials = it->second.get();
    } else {
        // special case where user_id is not defined.
        // Acceptable iff repository contains a single user for this app

        // loop over all credentials:
        // remember that this implementation is not to be used in production
        auto prefix = GetAppPrefix(app_info);
        for (auto it = user_credentials_map_.begin();
             it != user_credentials_map_.end();
             ++it) {
            std::string key = it->first;
            if (boost::starts_with(key, prefix)) {
                // Found:
                actual_user_id = key.substr(prefix.length());
                if (p_credentials != nullptr) {  // already found ?
                    BOOST_THROW_EXCEPTION(
                        std::invalid_argument("Several user credentials found"
                                              " for application " + prefix));
                }
                p_credentials = it->second.get();
                break;
            }
        }
        if (p_credentials == nullptr) {
            BOOST_THROW_EXCEPTION(
                std::invalid_argument("No user credentials found"
                                      " for application " + prefix));
        }
    }
    std::unique_ptr<UserCredentials> p_ret(
                new UserCredentials(app_info, actual_user_id, *p_credentials));
    return p_ret;
}

void UserCredentialsFileRepository::Save(
                                    const UserCredentials& user_credentials) {
    std::lock_guard<std::mutex> guard(lock_);

    auto key = GetUserKey(user_credentials.app_info(),
                          user_credentials.user_id());
    // Update map; we copy given credentials
    std::unique_ptr<Credentials> p_copy(user_credentials.credentials().Clone());
    user_credentials_map_[key] = std::move(p_copy);
    // Serialize to file:
    WriteUserCredentialsFile();
}

UserCredentialsFileRepository::~UserCredentialsFileRepository() {
}

void UserCredentialsFileRepository::ReadUserCredentialsFile() {
    LOG_DEBUG << "Will read UserCredentialsFile: " << path_;
    boost::filesystem::ifstream infofile(path_);
    if (infofile.fail()) {
        std::system_error se(errno, std::system_category());
        const char *p_msg = se.what();
        BOOST_THROW_EXCEPTION(std::ios_base::failure(
                std::string("Could not open file: ")
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

        std::string key = line.substr(0, ieq);
        boost::algorithm::trim(key);
        std::string credentials_value = line.substr(ieq+1);
        boost::algorithm::trim(credentials_value);
        // LOG_TRACE << "key=" << key;
        // LOG_TRACE << "credentials=" << credentials_value;
        auto p_credentials = std::unique_ptr<Credentials>(
                            Credentials::CreateFromJson(credentials_value));

        user_credentials_map_[key] = std::move(p_credentials);
        LOG_TRACE << "Read credentials for user: " << key;
    }
}

void UserCredentialsFileRepository::WriteUserCredentialsFile() const {
    LOG_DEBUG << "Writing user credentials file to " << path_;
    boost::filesystem::path temp_path = path_;
    temp_path += ".tmp";
    boost::filesystem::ofstream infofile(temp_path);
    if (infofile.fail()) {
        std::system_error se(errno, std::system_category());
        const char *p_msg = se.what();
        BOOST_THROW_EXCEPTION(std::ios_base::failure(
            std::string("Could not open file: ")
                    + utility::conversions::to_utf8string(temp_path.c_str())
                    + ": " + p_msg));
    }

    for (auto it = user_credentials_map_.begin();
         it != user_credentials_map_.end();
         ++it) {
        auto key = it->first;
        auto json = it->second->ToJsonString();
        infofile << key << "=" << json << "\n";
    }
    infofile.flush();
    infofile.close();
    if (!infofile.good()) {
        LOG_ERROR << "Could not write file: " << temp_path;
        BOOST_THROW_EXCEPTION(std::ios_base::failure(
                            "Could not write file: " + temp_path.string()));
    }

    // Rename to final name (need to delete first on windows):
    boost::filesystem::remove(path_);
    boost::filesystem::rename(temp_path, path_);
}


}  // namespace pcs_api
