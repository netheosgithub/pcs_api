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
#include <memory>
#include <list>
#include <chrono>
#include <thread>
#include <functional>
#include <mutex>

#include "boost/thread.hpp"
#include "boost/asio.hpp"

#include "gtest/gtest.h"

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/storage_facade.h"
#include "pcs_api/file_byte_sink.h"
#include "pcs_api/memory_byte_source.h"
#include "pcs_api/memory_byte_sink.h"
#include "pcs_api/c_exceptions.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/logger.h"

#include "misc_test_utils.h"
#include "functional_base_test.h"

namespace pcs_api {

class StressTest : public FunctionalTest {
 public:
    void TestCrud(std::shared_ptr<IStorageProvider> p_storage,
                  bool catch_exceptions);
};

INSTANTIATE_TEST_CASE_P(
  Providers,
  StressTest,
  ::testing::ValuesIn(g_providers_to_be_tested));


// gtest is not thread safe under windows, this is wrapper macro for assertions:
static std::mutex gtest_lock;
#define WITH_LOCKED_GTEST(x) { \
    std::lock_guard<std::mutex> lock(gtest_lock); \
    x \
}
static void UploadAndCheckRandomFiles(
                                std::shared_ptr<IStorageProvider> p_storage,
                                const CPath& test_root_path);
static std::list<std::shared_ptr<CBlob>> RecursivelyListBlobs(
                                std::shared_ptr<IStorageProvider> p_storage,
                                const CPath& path);

void StressTest::TestCrud(std::shared_ptr<IStorageProvider> p_storage,
                          bool catch_exceptions) {
    try {
        std::chrono::steady_clock::time_point start =
                                            std::chrono::steady_clock::now();
        LOG_INFO << "Test starting time=" << start.time_since_epoch().count();
        std::chrono::steady_clock::time_point now = start;
        // if start > now then time has gone back:
        // we give up instead of being trapped in a potentially very long loop
        while (start <= now && (now - start) < g_test_duration) {
            LOG_INFO << "============= Thread " << std::this_thread::get_id()
                << ": (elapsed="
                << (std::chrono::duration_cast<std::chrono::seconds>(now - start)).count()
                << " < " << g_test_duration.count() << " s) ================";
            p_storage->GetUserId();  // only to request hubic API
            WithRandomTestPath([&](CPath path) {
                UploadAndCheckRandomFiles(p_storage, path);
            });
            now = std::chrono::steady_clock::now();
        }
    } catch (...) {
        if (catch_exceptions) {
            // In case tests are multithreaded, exceptions are not catched
            // by gtest and would terminate process. So we handle them here
            WITH_LOCKED_GTEST(
                LOG_ERROR << "Catched an exception in test thread: "
                          << CurrentExceptionToString();
                FAIL() << "Thread failed with exception: "
                       << CurrentExceptionToString();
            )
        } else {
            // usual case of monothreaded tests
            throw;
        }
    }
}

static void UploadAndCheckRandomFiles(
                                std::shared_ptr<IStorageProvider> p_storage,
                                const CPath& test_root_path) {
    std::default_random_engine rnd;  // not shared (in case multi-thread)
    std::uniform_int_distribution<int> distrib0_999(0, 999);
    CPath tmp_path = test_root_path;

    for (int i = 0; i < MiscUtils::Random(0, 4) + 1; i++) {
        CPath path = MiscUtils::GenerateTestPath(tmp_path);
        if (utilities::Random() < 0.5) {
            // Create folder :
            p_storage->CreateFolder(path);
            // And sometimes go inside :
            if (utilities::Random() < 0.5) {
                tmp_path = path;
            }
        } else {
            // Create file (deterministic content for later check):
            std::hash<std::string> hash;
            size_t h = hash(path.path_name_utf8());
            rnd.seed(static_cast<unsigned long>(h));  // NOLINT(runtime/int)
            // prefer small files:
            int file_size = distrib0_999(rnd) * distrib0_999(rnd);
            std::string data = MiscUtils::GenerateRandomData(file_size, &rnd);
            std::shared_ptr<ByteSource> p_bsource =
                                    std::make_shared<MemoryByteSource>(data);
            CUploadRequest ur(path, p_bsource);
            p_storage->Upload(ur);
        }
    }

    // Check blob files content :
    std::list<std::shared_ptr<CBlob>> all_blobs =
                            RecursivelyListBlobs(p_storage, test_root_path);
    LOG_INFO << "Uploaded " << all_blobs.size() << " blobs";
    for (std::shared_ptr<CBlob> p_blob : all_blobs) {
        std::hash<std::string> hash;
        size_t h = hash(p_blob->path().path_name_utf8());
        rnd.seed(static_cast<unsigned long>(h));  // NOLINT(runtime/int)
        // same formula as above:
        int file_size = distrib0_999(rnd) * distrib0_999(rnd);
        WITH_LOCKED_GTEST(
          ASSERT_EQ(file_size, p_blob->length());
        )
        std::string expected_data =
                                MiscUtils::GenerateRandomData(file_size, &rnd);
        std::shared_ptr<MemoryByteSink> p_bsink =
                                            std::make_shared<MemoryByteSink>();
        CDownloadRequest dr(p_blob->path(), p_bsink);
        p_storage->Download(dr);
        WITH_LOCKED_GTEST(
          ASSERT_EQ(expected_data, p_bsink->GetData());
        )
        LOG_INFO << "Checked blob: " << *p_blob;
    }
}

static std::list<std::shared_ptr<CBlob>> RecursivelyListBlobs(
                                    std::shared_ptr<IStorageProvider> p_storage,
                                    const CPath& path) {
    std::list<std::shared_ptr<CBlob>> ret_list;
    std::shared_ptr<CFolderContent> files = p_storage->ListFolder(path);
    for (auto it = files->cbegin() ; it != files->cend() ; ++it) {
        if (it->second->IsBlob()) {
            ret_list.push_back(std::dynamic_pointer_cast<CBlob>(it->second));
        } else {
            ret_list.splice(ret_list.end(),
                           RecursivelyListBlobs(p_storage, it->second->path()));
        }
    }
    return ret_list;
}

TEST_P(StressTest, TestCrud) {
    TestCrud(p_storage_, false);
}

TEST_P(StressTest, TestMultiCrud) {
    boost::asio::io_service io_service;
    boost::thread_group threads;
    std::unique_ptr<boost::asio::io_service::work> work(
                                new boost::asio::io_service::work(io_service));

    // Spawn worker threads
    for (std::size_t i = 0; i < g_nb_threads; ++i) {
        threads.create_thread(
                    boost::bind(&boost::asio::io_service::run, &io_service));
    }

    // Post the tasks to the io_service
    for (std::size_t i = 0; i < g_nb_threads; ++i) {
        io_service.post(boost::bind(&StressTest::TestCrud,
                                    this, p_storage_, true));
    }
    // Let some time for threads to start, before stopping service:
    std::this_thread::sleep_for(std::chrono::seconds(5));
    io_service.stop();  // will not accept any job by now
    threads.join_all();
}


}  // namespace pcs_api

