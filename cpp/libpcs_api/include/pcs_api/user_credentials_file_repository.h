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

#ifndef INCLUDE_PCS_API_USER_CREDENTIALS_FILE_REPOSITORY_H_
#define INCLUDE_PCS_API_USER_CREDENTIALS_FILE_REPOSITORY_H_

#include <string>
#include <map>
#include <mutex>

#include "pcs_api/user_credentials_repository.h"

namespace pcs_api {

/**
 * \brief An implementation of UserCredentialsRepository based on flat file.
 *
 * This class is thread safe, but NOT multi-process safe.
 *
 * This class is provided only for development purposes, user of libpcs_api
 * is in charge of a more robust and scalable implementation.
 */
class UserCredentialsFileRepository : public UserCredentialsRepository {
 public:
    explicit UserCredentialsFileRepository(const boost::filesystem::path& path);
    std::unique_ptr<UserCredentials> Get(
                                const AppInfo &app_info,
                                const std::string& user_id) const override;
    void Save(const UserCredentials& user_credentials) override;
    ~UserCredentialsFileRepository();

 private:
    boost::filesystem::path path_;
    mutable std::mutex lock_;
    std::map<std::string, std::unique_ptr<Credentials>> user_credentials_map_;
    void ReadUserCredentialsFile();
    void WriteUserCredentialsFile() const;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_USER_CREDENTIALS_FILE_REPOSITORY_H_

