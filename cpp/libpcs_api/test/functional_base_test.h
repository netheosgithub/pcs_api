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

#ifndef INCLUDE_TEST_FUNCTIONAL_BASE_TEST_H_
#define INCLUDE_TEST_FUNCTIONAL_BASE_TEST_H_

#include <string>
#include <memory>
#include <vector>

#include "pcs_api/i_storage_provider.h"

// defined in main:
extern std::vector<std::string> g_providers_to_be_tested;
extern std::chrono::seconds g_test_duration;
extern unsigned int g_nb_threads;

namespace pcs_api {

/**
 * \brief Base class for providers real tests.
 *
 * gtest parameter is provider name.
 */
class FunctionalTest : public ::testing::TestWithParam<std::string> {
 public:
    FunctionalTest();
    void SetUp();
    void TearDown();

    /**
     * \brief a simple around test invoke: temp folder is created before test,
     * passed to test function, then deleted.
     */
    void WithRandomTestPath(std::function<void(CPath)> test_func);

 protected:
    std::shared_ptr<AppInfoRepository> p_app_repo_;
    std::shared_ptr<UserCredentialsRepository> p_user_repo_;
    std::shared_ptr<IStorageProvider> p_storage_;

 private:
    void CreateRepositories();
    std::shared_ptr<IStorageProvider> CreateProvider(
                                            const std::string& provider_name);
};

}  // namespace pcs_api

#endif  // INCLUDE_TEST_FUNCTIONAL_BASE_TEST_H_
