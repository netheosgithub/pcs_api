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

#include <string>
#include <random>

#include "boost/algorithm/string/predicate.hpp"

#include "pcs_api/i_storage_provider.h"
#include "pcs_api/internal/logger.h"
#include "pcs_api/internal/utilities.h"

#include "misc_test_utils.h"

namespace pcs_api {

const std::chrono::seconds MiscUtils::kTimeAllowedDelta =
                                                    std::chrono::seconds(120);
const string_t MiscUtils::kTestFolderPrefix =
                                        PCS_API_STRING_T("/pcs_api_tmptest_");


bool MiscUtils::IsDatetimeAlmostNow(const ::boost::posix_time::ptime& date) {
    return IsDatetimeAlmostEquals(
                ::boost::posix_time::second_clock::universal_time(), date);
}

bool
MiscUtils::IsDatetimeAlmostEquals(const ::boost::posix_time::ptime& expected,
                                  const ::boost::posix_time::ptime& actual) {
    boost::posix_time::time_duration diff = expected - actual;
    bool ret = std::abs(diff.total_seconds()) < kTimeAllowedDelta.count();
    if (!ret) {
        LOG_INFO << "Times are very different : expected="
                 << expected << " but actual=" << actual;
    }
    return ret;
}

void
MiscUtils::CleanupTestFolders(std::shared_ptr<IStorageProvider> p_storage) {
    std::shared_ptr<CFolderContent> p_cfc = p_storage->ListRootFolder();
    for (auto it = p_cfc->cbegin(); it != p_cfc->cend(); ++it) {
        CPath path = it->first;
        if (boost::starts_with(path.path_name(), kTestFolderPrefix)) {
            LOG_INFO << "Deleting old test folder: " << path;
            p_storage->Delete(path);
        }
    }
}

CPath MiscUtils::GenerateTestPath() {
    return GenerateTestPath(CPath(PCS_API_STRING_T("/")));
}

CPath MiscUtils::GenerateTestPath(const CPath& parent) {
    string_t temp_pathname;
    for (int i = 0; i < 6; i++) {
        temp_pathname += (PCS_API_STRING_T('A')
                          + static_cast<int>(utilities::Random()*26));
    }
    if (parent.IsRoot()) {
        return CPath(kTestFolderPrefix + temp_pathname);
    } else {
        return parent.Add(temp_pathname);
    }
}

/**
 * \brief A shared generator, seeded with different values at start time
 * so that tests are not deterministic.
 */
static std::default_random_engine* GetRandomGenerator() {
    static std::default_random_engine shared_generator;
    static bool seeded = false;
    if (!seeded) {
        unsigned long s = static_cast<unsigned long>(time(NULL));  // NOLINT
        shared_generator.seed(s);
        seeded = true;
    }
    return &shared_generator;
}

std::string MiscUtils::GenerateRandomData(size_t size) {
    return GenerateRandomData(size, GetRandomGenerator());
}

std::string MiscUtils::GenerateRandomData(size_t size,
                                          std::default_random_engine *p_rnd) {
    std::string ret;
    ret.resize(size);
    for (unsigned int i = 0; i < size ; ++i) {
        ret[i] = (*p_rnd)();
    }
    return ret;
}

int MiscUtils::Random(int start, int end) {
    return start + static_cast<int>((end-start)*utilities::Random());
}


}  // namespace pcs_api


