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
#include "pcs_api/internal/multipart_streambuf.h"
#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/logger.h"

#include "misc_test_utils.h"

namespace pcs_api {

static void RandomlyStreamToString(std::istream *p_istream,
                                   std::ostringstream *p_dest,
                                   bool may_interrupt_before_end) {
    int max_read_size = MiscUtils::Random(1, 2000);
    std::unique_ptr<char[]> p_data(new char[max_read_size]);
    while (true) {
        int read_size = MiscUtils::Random(1, max_read_size);
        p_istream->read(p_data.get(), read_size);
        std::streamsize n = p_istream->gcount();
        if (n <= 0) {
            break;
        }
        p_dest->write(p_data.get(), n);
        if (may_interrupt_before_end && utilities::Random() < 0.2) {
            // we stop reading now, may be before end of data
            break;
        }
    }
}

TEST(MultipartStreambufTest, TellAndSeek) {
    // Prepare multipart:
    MemoryByteSource mbs1("Hello, I am 20 bytes");
    MultipartStreamer ms("related", "myboundary");
    MultipartStreamer::Part p1("name1", &mbs1);
    p1.AddHeader("Content-Disposition",
                 "form-data; name=\"file1\"; filename=\"my_file_name...\"");
    p1.AddHeader("Content-Type", "text/plain");
    ms.AddPart(p1);

    MultipartStreambuf msb(&ms);
    std::istream is(&msb);
    EXPECT_EQ(0, is.tellg());
    is.seekg(0);
    EXPECT_EQ(0, is.tellg());
    // check we cannot seek in the middle:
    LOG_INFO << "An error is expected below:";
    is.seekg(10);  // not allowed
    EXPECT_TRUE(is.fail());
    is.clear();
    EXPECT_EQ(0, is.tellg());

    // Read whole content once:
    std::ostringstream data;
    RandomlyStreamToString(&is, &data, false);
    EXPECT_EQ((int64_t)data.str().length(), ms.ContentLength());
    is.clear();
    EXPECT_EQ(ms.ContentLength(), is.tellg());
    is.seekg(0);
    EXPECT_EQ(0, is.tellg());
}

TEST(MultipartStreambufTest, QuickFuzz) {
    for (int n = 0; n < 1000; n++) {
        // Prepare multipart:
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

        // Wrap streamer as streambuf, then as istream:
        MultipartStreambuf msb(&ms);
        std::istream is(&msb);
        EXPECT_FALSE(is.fail());

        // Read whole content once:
        is.seekg(0);
        std::ostringstream data;
        RandomlyStreamToString(&is, &data, false);
        EXPECT_EQ((int64_t)data.str().length(), ms.ContentLength());
        is.clear();
        EXPECT_EQ(ms.ContentLength(), is.tellg());

        // What happens if we interrupt before end of process ?
        std::string msg = "length1=" + std::to_string(length1)
            + "  length2=" + std::to_string(length2);
        SCOPED_TRACE(msg);
        // check we can seek to beginning:
        is.clear();
        is.seekg(0);
        EXPECT_EQ(0, is.tellg());

        std::ostringstream truncated_data;
        RandomlyStreamToString(&is, &truncated_data, true);
        std::string truncated_string = truncated_data.str();
        EXPECT_TRUE((int64_t)truncated_string.length() <= ms.ContentLength());
        EXPECT_EQ(data.str().substr(0, truncated_string.length()),
                  truncated_string);

        // Check we can read again whole content:
        is.clear();
        is.seekg(0);
        EXPECT_EQ(0, is.tellg());
        std::ostringstream data2;
        RandomlyStreamToString(&is, &data2, false);
        EXPECT_EQ(data.str(), data2.str());
        EXPECT_EQ(data2.str().length(), ms.ContentLength());
    }
}

}  // namespace pcs_api

