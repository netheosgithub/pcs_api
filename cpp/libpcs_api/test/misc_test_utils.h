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

#ifndef INCLUDE_TEST_MISC_TEST_UTILS_H_
#define INCLUDE_TEST_MISC_TEST_UTILS_H_

#include <string>
#include <memory>
#include <random>

#include "boost/date_time/posix_time/posix_time.hpp"

#include "pcs_api/i_storage_provider.h"

/**
 * Random functions of this class are not to be used for cryptographic purpose.
 */


/**
 * Google test can not dynamically ignore tests: that's a pity.
 * Ignored tests will be reported as success, but we try to track them anyway.
 */
extern unsigned int g_nb_ignored_tests;

#define NOT_SUPPORTED_BY_PROVIDER(p_storage, provider_name, msg) \
    if (p_storage->GetProviderName() == provider_name) { \
        std::cout << std::endl \
                  << "IGNORED test for provider " << provider_name \
                  << ": " << msg \
                  << std::endl << std::endl; \
        ++g_nb_ignored_tests; \
        return; \
    }

#define NOT_SUPPORTED_BY_OS(os_name, msg) \
    { \
        const ::testing::TestInfo* const p_test_info = \
            ::testing::UnitTest::GetInstance()->current_test_info(); \
        std::cout << std::endl \
                  << "IGNORED test " << p_test_info->test_case_name() \
                  << "." << p_test_info->name() \
                  << ": not supported on OS " << os_name \
                  << std::endl << std::endl; \
        ++g_nb_ignored_tests; \
        return; \
    }

namespace pcs_api {

/**
 * Some miscellaneous test-related functions.
 */
class MiscUtils {
 public:
    static const std::chrono::seconds kTimeAllowedDelta;
    static const string_t kTestFolderPrefix;

    static bool IsDatetimeAlmostNow(const ::boost::posix_time::ptime& date);
    static bool IsDatetimeAlmostEquals(
                                const ::boost::posix_time::ptime& expected,
                                const ::boost::posix_time::ptime& actual);

    /**
     * \brief Cleanup any test folders that may still exist at the end of tests.
     *
     * @param storage The storage to clean
     */
    static void CleanupTestFolders(std::shared_ptr<IStorageProvider> p_storage);

    /**
     * \brief Generate a temp folder path for tests.
     *
     * @return a path with random name (but constant prefix)
     */
    static CPath GenerateTestPath();

    /**
     * \brief Generate a temp folder path for tests.
     *
     * @param parent returned path parent folder
     * @return a path with random name (but constant prefix and given parent)
     */
    static CPath GenerateTestPath(const CPath& parent);

    /**
     * \brief Return a string composed of random data (possibly including \0)
     *        with specified size.
     *
     * @param size
     * @return a string of random bytes and given length
     */
    static std::string GenerateRandomData(size_t size);

    /**
     * \brief Return a string composed of (deterministic) random data with
     *        specified size.
     *
     * @param size
     * @param p_rnd
     * @return if *p_rnd is not fully random, deterministic content is returned
     */
    static std::string GenerateRandomData(size_t size,
                                          std::default_random_engine *p_rnd);

    /**
     * \brief Generate a random number in [start;end) interval
     *        (end is excluded).
     *
     * @param start the lowest possible returned value
     * @param end the highest+1 possible returned value
     * @return (pseudo) random value (if start==end, return always 'start')
     */
    static int Random(int start, int end);
};

}  // namespace pcs_api

#endif  // INCLUDE_TEST_MISC_TEST_UTILS_H_
