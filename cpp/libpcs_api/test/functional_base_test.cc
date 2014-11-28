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

#include <algorithm>
#include <functional>

#include "gtest/gtest.h"

#include "cpprest/asyncrt_utils.h"
#include "cpprest/http_client.h"

#include "pcs_api/model.h"
#include "pcs_api/storage_facade.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/app_info_file_repository.h"
#include "pcs_api/user_credentials_file_repository.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/storage_provider.h"
#include "pcs_api/internal/logger.h"

#include "functional_base_test.h"
#include "misc_test_utils.h"

namespace pcs_api {

FunctionalTest::FunctionalTest() {
    CreateRepositories();
}

void FunctionalTest::SetUp() {
    p_storage_ = CreateProvider(GetParam());
    LOG_INFO << "Starting test for provider: " << p_storage_->GetProviderName();
}

void FunctionalTest::TearDown() {
    if (p_storage_) {
        LOG_INFO << "Finished test for provider: "
                 << p_storage_->GetProviderName();
    }
}

void FunctionalTest::CreateRepositories() {
    const char *p_pcs_api_repo_dir = getenv("PCS_API_REPOSITORY_DIR");
    if (p_pcs_api_repo_dir == NULL) {
        p_pcs_api_repo_dir = "../../repositories";
    }
    boost::filesystem::path pcs_api_repo_path(p_pcs_api_repo_dir);
    p_app_repo_ = std::make_shared<AppInfoFileRepository>(
                                    pcs_api_repo_path / "app_info_data.txt");
    p_user_repo_ = std::make_shared<UserCredentialsFileRepository>(
                               pcs_api_repo_path / "user_credentials_data.txt");
}

std::shared_ptr<IStorageProvider> FunctionalTest::CreateProvider(
                                            const std::string& provider_name) {
    StorageBuilder builder = StorageFacade::ForProvider(provider_name)
            .app_info_repository(p_app_repo_, "")
            .user_credentials_repository(p_user_repo_, "");
    // Example of manual proxy configuration:
    // we modify builder default http_client_config that has proper timeout, etc
    // web::web_proxy proxy(web::uri(U("https://10.0.0.1:3128")));
    // proxy.set_credentials(web::credentials(U("user"), U("password")));
    // builder.http_client_config()->set_proxy(proxy);
    std::shared_ptr<IStorageProvider> p_storage = builder.Build();
    return p_storage;
}

static void DeleteQuietly(CPath path,
                          std::shared_ptr<IStorageProvider> p_storage) {
    try {
        p_storage->Delete(path);
    }
    catch (std::exception&) {
        LOG_WARN << "Error during cleanup: deleting file "
                 << path << ": " << CurrentExceptionToString();
    }
}

void FunctionalTest::WithRandomTestPath(std::function<void(CPath)> test_func) {
    CPath temp_root_path = MiscUtils::GenerateTestPath();
    LOG_INFO << "Will use test folder: " << temp_root_path;
    std::exception_ptr p_exception;
    try {
        test_func(temp_root_path);
    } catch (...) {
        p_exception = std::current_exception();
        LOG_ERROR << "Test failed with exception: "
                  << ExceptionPtrToString(p_exception);
    }
    DeleteQuietly(temp_root_path, p_storage_);
    if (p_exception) {
        std::rethrow_exception(p_exception);
    }
}

}  // namespace pcs_api

