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
#include <sstream>

#include "gtest/gtest.h"

#include "pcs_api/memory_byte_source.h"
#include "pcs_api/internal/multipart_streamer.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/logger.h"

#include "misc_test_utils.h"

namespace pcs_api {

static void StreamToString(MultipartStreamer* p_streamer,
                           std::ostringstream *p_dest) {
    char buf[1024];
    while (true) {
        std::streamsize n = p_streamer->ReadData(buf, sizeof(buf));
        if (n == 0) {  // reached eof
            // We can try to read again, still getting 0:
            n = p_streamer->ReadData(buf, sizeof(buf));
            EXPECT_EQ(0, n);
            break;
        }
        if (n < 0) {
            FAIL();
        }
        p_dest->write(buf, n);
    }
}

TEST(MultipartStreamerTest, OnePart) {
    MemoryByteSource mbs1("Hello, I am 20 bytes");
    MultipartStreamer ms("related", "myboundary");
    MultipartStreamer::Part p1("name1", &mbs1);
    p1.AddHeader("Content-Disposition",
                 "form-data; name=\"file1\"; filename=\"my_file_name...\"");
    p1.AddHeader("Content-Type", "text/plain");
    ms.AddPart(p1);

    std::ostringstream data;
    StreamToString(&ms, &data);
    std::string expected =
        "--myboundary\r\n"
        "Content-Disposition: form-data;"
            " name=\"file1\"; filename=\"my_file_name...\"\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Hello, I am 20 bytes"
        "\r\n--myboundary--\r\n";
    EXPECT_EQ(expected, data.str());
    EXPECT_EQ(PCS_API_STRING_T("multipart/related; boundary=myboundary"),
              ms.ContentType());
    EXPECT_EQ(expected.length(), ms.ContentLength());

    // We can reset and get data again:
    ms.Reset();
    data.str("");
    StreamToString(&ms, &data);
    EXPECT_EQ(expected, data.str());
    EXPECT_EQ(PCS_API_STRING_T("multipart/related; boundary=myboundary"),
              ms.ContentType());
    EXPECT_EQ(expected.length(), ms.ContentLength());
}

TEST(MultipartStreamerTest, OneEmptyPart) {
    MemoryByteSource empty_source("");
    MultipartStreamer ms("form-data", "a_boundary");
    MultipartStreamer::Part p1("name1", &empty_source);
    p1.AddHeader("Content-Disposition",
                 "form-data; name=\"file1\"; filename=\"my_file_name...\"");
    p1.AddHeader("Content-Type", "text/plain");
    ms.AddPart(p1);

    std::ostringstream data;
    StreamToString(&ms, &data);
    std::string expected =
        "--a_boundary\r\n"
        "Content-Disposition: form-data;"
            " name=\"file1\"; filename=\"my_file_name...\"\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "\r\n--a_boundary--\r\n";
    EXPECT_EQ(expected, data.str());
    EXPECT_EQ(PCS_API_STRING_T("multipart/form-data; boundary=a_boundary"),
              ms.ContentType());
    EXPECT_EQ(expected.length(), ms.ContentLength());

    // We can reset and get data again:
    ms.Reset();
    data.str("");
    StreamToString(&ms, &data);
    EXPECT_EQ(expected, data.str());
    EXPECT_EQ(PCS_API_STRING_T("multipart/form-data; boundary=a_boundary"),
              ms.ContentType());
    EXPECT_EQ(expected.length(), ms.ContentLength());
}

TEST(MultipartStreamerTest, TwoParts) {
    MemoryByteSource mbs1("Hello, I am 20 bytes");
    MultipartStreamer ms("related", "myboundary");
    MultipartStreamer::Part p1("name1", &mbs1);
    p1.AddHeader("Content-Disposition",
                 "form-data; name=\"file1\"; filename=\"my_file_name...\"");
    ms.AddPart(p1);

    std::string file_content2 = MiscUtils::GenerateRandomData(10000);
    MemoryByteSource mbs2(file_content2);
    MultipartStreamer::Part p2("name2", &mbs2);
    ms.AddPart(p2);

    std::string expected =
        "--myboundary\r\n"
        "Content-Disposition: form-data; name=\"file1\";"
            " filename=\"my_file_name...\"\r\n\r\n"
        "Hello, I am 20 bytes"
        "\r\n--myboundary\r\n"
        "\r\n"  // no headers
        + file_content2 +
        "\r\n--myboundary--\r\n";
    EXPECT_EQ(PCS_API_STRING_T("multipart/related; boundary=myboundary"),
              ms.ContentType());
    EXPECT_EQ(expected.length(), ms.ContentLength());

    std::ostringstream data;
    StreamToString(&ms, &data);
    EXPECT_EQ(expected, data.str());

    // Check this is repeatable:
    ms.Reset();
    EXPECT_EQ(PCS_API_STRING_T("multipart/related; boundary=myboundary"),
              ms.ContentType());
    EXPECT_EQ(expected.length(), ms.ContentLength());
    data.str("");
    StreamToString(&ms, &data);
    EXPECT_EQ(expected, data.str());
}

static void RandomlyStreamToString(MultipartStreamer* p_streamer,
                                   std::ostringstream *p_dest,
                                   bool may_interrupt_before_end) {
    int max_read_size = MiscUtils::Random(1, 2000);
    std::unique_ptr<char[]> p_data(new char[max_read_size]);
    while (true) {
        int read_size = MiscUtils::Random(1, max_read_size);
        std::streamsize n = p_streamer->ReadData(p_data.get(), read_size);
        if (n == 0) {
            // We can try to read again, still getting 0:
            n = p_streamer->ReadData(p_data.get(), read_size);
            EXPECT_EQ(0, n);
            break;
        }
        if (n < 0) {
            FAIL();
        }
        p_dest->write(p_data.get(), n);
        if (may_interrupt_before_end && utilities::Random() < 0.2) {
            // we stop reading now, may be before end of data
            break;
        }
    }
}

TEST(MultipartStreamerTest, QuickFuzz) {
    for (int n = 0; n < 1000; n++) {
        int length1 = MiscUtils::Random(0, 10000);
        if (utilities::Random() < 0.05) {
            length1 = 0;
        }
        std::string file_content1 = MiscUtils::GenerateRandomData(length1);
        MemoryByteSource mbs1(file_content1);
        MultipartStreamer ms;
        MultipartStreamer::Part p1("name1", &mbs1);
        p1.AddHeader("Content-Disposition",
                     "form-data; name=\"file1\"; filename=\"my_file_name...\"");
        ms.AddPart(p1);

        int length2 = MiscUtils::Random(0, 10000);
        if (utilities::Random() < 0.05) {
            length2 = 0;
        }
        std::string file_content2 = MiscUtils::GenerateRandomData(length2);
        MemoryByteSource mbs2(file_content2);
        MultipartStreamer::Part p2("name2", &mbs2);
        p2.AddHeader("Content-Type", "application/octet-stream");
        ms.AddPart(p2);

        // Read whole content once:
        std::ostringstream data;
        RandomlyStreamToString(&ms, &data, false);
        EXPECT_EQ((int64_t)data.str().length(), ms.ContentLength());

        // What happens if we interrupt before end of process ?
        std::string msg = "length1=" + std::to_string(length1)
                        + "  length2=" + std::to_string(length2);
        SCOPED_TRACE(msg);
        ms.Reset();
        std::ostringstream truncated_data;
        RandomlyStreamToString(&ms, &truncated_data, true);
        std::string truncated_string = truncated_data.str();
        EXPECT_TRUE((int64_t)truncated_string.length() <= ms.ContentLength());
        EXPECT_EQ(data.str().substr(0, truncated_string.length()),
                  truncated_string);

        // Check we can read again whole content:
        ms.Reset();
        std::ostringstream data2;
        RandomlyStreamToString(&ms, &data2, false);
        EXPECT_EQ(data.str(), data2.str());
        EXPECT_EQ(data2.str().length(), ms.ContentLength());
    }
}

}  // namespace pcs_api

