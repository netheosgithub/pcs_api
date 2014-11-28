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

#include "boost/filesystem.hpp"
#include "boost/filesystem/fstream.hpp"

#include "gtest/gtest.h"

#include "pcs_api/model.h"
#include "pcs_api/stdout_progress_listener.h"
#include "pcs_api/file_byte_sink.h"
#include "pcs_api/file_byte_source.h"
#include "pcs_api/memory_byte_sink.h"
#include "pcs_api/memory_byte_source.h"
#include "pcs_api/internal/progress_byte_sink.h"
#include "pcs_api/internal/progress_byte_source.h"
#include "pcs_api/internal/logger.h"
#include "pcs_api/internal/utilities.h"
#include "misc_test_utils.h"


namespace pcs_api {

class BytesIOTest : public ::testing::Test {
 public:
    void SetUp() override {
        tmp_dir_ = boost::filesystem::unique_path("pcs_api_%%%%%%%%.dir");
        boost::filesystem::remove(tmp_dir_);
        boost::filesystem::create_directory(tmp_dir_);
    }

    void TearDown() override {
        boost::filesystem::remove_all(tmp_dir_);
    }

 protected:
    boost::filesystem::path tmp_dir_;
    const std::string kByteContent = "This 1\xE2\x82\xAC file is the test"
                            " content of a file byte source... (70 bytes)";
};

static void WriteStringToFile(const std::string& str,
                              boost::filesystem::path dest_file_path) {
    boost::filesystem::ofstream os(dest_file_path, std::ios::binary);
    os.write(str.data(), str.length());
    os.close();
    ASSERT_FALSE(os.fail());
}

static void WriteStringToStream(const std::string& str, std::ostream *p_os) {
    p_os->write(str.data(), str.length());
    ASSERT_FALSE(p_os->fail());
}

static std::string ConsumeStreamToString(std::istream *p_is) {
    std::string ret;
    char buffer[1024];
    while (p_is->read(buffer, sizeof(buffer)))
        ret.append(buffer, sizeof(buffer));
    // between 0 and sizeof(buffer): fits in size_t:
    ret.append(buffer, (size_t)p_is->gcount());
    return ret;
}

static void CheckByteSource(std::shared_ptr<ByteSource> p_bs,
                            const std::string& expected_content) {
    EXPECT_EQ(expected_content.length(), p_bs->Length());

    std::unique_ptr<std::istream> p_is = p_bs->OpenStream();
    std::string read_content = ConsumeStreamToString(p_is.get());
    p_is.reset();
    EXPECT_EQ(expected_content, read_content);

    // Now decorate with a progress byte source
    std::shared_ptr<StdoutProgressListener> p_pl =
                            std::make_shared<StdoutProgressListener>(false);
    detail::ProgressByteSource progress_bs(p_bs, p_pl);
    EXPECT_EQ(expected_content.length(), progress_bs.Length());
    p_is = progress_bs.OpenStream();
    EXPECT_EQ(p_bs->Length(), p_pl->total());
    EXPECT_EQ(0, p_pl->current());
    EXPECT_FALSE(p_pl->is_aborted());

    // randomize read size:
    unsigned int read_size = MiscUtils::Random(1, 2048);
    LOG_DEBUG << "Testing will read size=" << read_size;
    std::vector<char> b(read_size);
    p_is->read(b.data(), 1);
    if (!expected_content.empty()) {
        EXPECT_GT(p_pl->current(), 0);  // at least one byte has been read
        EXPECT_LT(p_pl->current(), 2048);  // not too many bytes have been read
    }
    int64_t last_current = p_pl->current();
    while (*p_is) {
        p_is->read(b.data(), b.capacity());
        // current always increases:
        EXPECT_GE(p_pl->current() - last_current, 0);
        // current does not increase too fast:
        EXPECT_LE(p_pl->current() - last_current, 2048);
        last_current = p_pl->current();
    }
    // At the end of stream, we should have seen the correct number of bytes:
    EXPECT_EQ(expected_content.length(), p_pl->current());
}

static void CheckByteSources(boost::filesystem::path tmp_file_path,
                             std::string expected_content) {
    WriteStringToFile(expected_content, tmp_file_path);

    std::shared_ptr<ByteSource> p_bs =
                            std::make_shared<FileByteSource>(tmp_file_path);
    CheckByteSource(p_bs, expected_content);

    p_bs = std::make_shared<MemoryByteSource>(expected_content);
    CheckByteSource(p_bs, expected_content);
}

TEST_F(BytesIOTest, TestByteSource) {
    auto tmp_path = tmp_dir_ / "byte_source.txt";
    CheckByteSources(tmp_path, kByteContent);
}

TEST_F(BytesIOTest, EmptyByteSource) {
    auto tmp_path = tmp_dir_ / "empty_byte_source.txt";
    CheckByteSources(tmp_path, std::string());
}

TEST_F(BytesIOTest, TestLongByteSource) {
    auto tmp_path = tmp_dir_ / "long_byte_source.txt";
    unsigned int size = MiscUtils::Random(0, 1000000);
    std::string data = MiscUtils::GenerateRandomData(size);
    CheckByteSources(tmp_path, data);
}

static void CheckFileByteSink(const std::string& data_to_write,
                              bool abort,
                              boost::filesystem::path pathname,
                              bool temp_name_during_writes,
                              bool delete_on_abort) {
    FileByteSink fbs(pathname, temp_name_during_writes, delete_on_abort);
    ByteSink *p_bs = &fbs;
    auto actual_pathname = pathname;
    if (temp_name_during_writes) {
        actual_pathname += ".part";
    }
    std::ostream* p_os = p_bs->OpenStream();
    // check file exists :
    EXPECT_TRUE(boost::filesystem::exists(actual_pathname));
    p_bs->SetExpectedLength(data_to_write.length());

    // write not all bytes, only beginning of data:
    p_os->write(data_to_write.data(), 10);
    p_os->flush();
    EXPECT_EQ(10, boost::filesystem::file_size(actual_pathname));

    if (abort) {
        LOG_INFO << "Aborting byte sink !";
        p_bs->Abort();
    }
    p_bs->CloseStream();

    bool file_still_exists = boost::filesystem::exists(pathname);
    bool temp_file_still_exists = boost::filesystem::exists(actual_pathname);
    if (!abort) {
        // operation has not been aborted : so file never deleted
        EXPECT_TRUE(file_still_exists);
    } else {
        // operation has been aborted :
        // if a temp file is used and not deleted, it has not been renamed :
        EXPECT_TRUE(temp_file_still_exists != delete_on_abort);
    }
}

TEST_F(BytesIOTest, TestFileByteSink) {
    auto tmp_path = tmp_dir_ / "file_byte_sink.txt";
    std::list<bool> booleans;
    booleans.push_back(true);
    booleans.push_back(false);

    for (const bool abort : booleans) {
        for (const bool temp_name_during_writes : booleans) {
            for (const bool delete_on_abort : booleans) {
                LOG_DEBUG << "CheckFileByteSinkAllFlags - abort: " << abort
                        << " - temp_name: " << temp_name_during_writes
                        << " - deleteOnAbort: " << delete_on_abort;
                CheckFileByteSink(kByteContent, abort, tmp_path,
                                  temp_name_during_writes, delete_on_abort);
            }
        }
    }
}

TEST_F(BytesIOTest, TestMemoryByteSink) {
    MemoryByteSink mb_sink;
    std::ostream* p_os = mb_sink.OpenStream();
    WriteStringToStream(kByteContent, p_os);
    mb_sink.CloseStream();

    std::string stored_data = mb_sink.GetData();
    EXPECT_EQ(kByteContent, stored_data);
}

static void CheckProgressByteSink(std::shared_ptr<ByteSink> p_sink,
                                  std::string expected_content,
                                  bool abort) {
    std::shared_ptr<StdoutProgressListener> p_pl =
                            std::make_shared<StdoutProgressListener>(false);
    detail::ProgressByteSink progress_bs(p_sink, p_pl);

    std::ostream* p_os = progress_bs.OpenStream();
    EXPECT_EQ(-1, p_pl->total());
    EXPECT_EQ(0, p_pl->current());
    EXPECT_FALSE(p_pl->is_aborted());

    progress_bs.SetExpectedLength(expected_content.length());
    EXPECT_EQ(expected_content.length(), p_pl->total());

    p_os->write(expected_content.data(), 1);
    p_os->flush();
    EXPECT_EQ(1, p_pl->current());

    p_os->write(expected_content.data()+1, expected_content.length()-1);
    p_os->flush();
    EXPECT_EQ(expected_content.length(), p_pl->current());
    EXPECT_EQ(expected_content.length(), p_pl->total());

    if (abort) {
        // in order to check that Abort() is forwarded:
        progress_bs.Abort();
    }
    progress_bs.CloseStream();
}

TEST_F(BytesIOTest, TestProgressByteSink) {
    auto tmp_path = tmp_dir_ / "byte_sink_progress.txt";

    std::shared_ptr<ByteSink> p_fbs =
            std::make_shared<FileByteSink>(tmp_path,
                                           false,  // temp_name_during_write
                                           true);  // delete_on_abort
    CheckProgressByteSink(p_fbs, kByteContent, false);  // do not abort
    // Check content match:
    EXPECT_TRUE(boost::filesystem::exists(tmp_path));
    EXPECT_EQ(kByteContent.length(), boost::filesystem::file_size(tmp_path));
    boost::filesystem::ifstream read_back(tmp_path);
    char data[1024];
    read_back.read(data, sizeof(data));
    std::streamsize nb_read = read_back.gcount();
    EXPECT_EQ(kByteContent.length(), nb_read);
    EXPECT_TRUE(read_back.eof());
    EXPECT_FALSE(read_back.bad());
    read_back.close();

    // Check Abort() is forwarded:
    CheckProgressByteSink(p_fbs, kByteContent, true);  // abort
    EXPECT_FALSE(boost::filesystem::exists(tmp_path));

    std::shared_ptr<MemoryByteSink> p_mbs = std::make_shared<MemoryByteSink>();
    CheckProgressByteSink(p_mbs, kByteContent, false);
    EXPECT_EQ(kByteContent, p_mbs->GetData());
}


}  // namespace pcs_api

