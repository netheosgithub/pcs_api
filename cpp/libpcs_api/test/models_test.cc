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

// Seems to be required because ayncrt_utils.h includes 
// <boost/algorithm/string.hpp> and errors occur if headers are included
// in wrong order
// Only required if including both gtest.h and progress_byte_*.h
#include "boost/iostreams/filtering_stream.hpp"

#include "gtest/gtest.h"

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/model.h"
#include "pcs_api/stdout_progress_listener.h"
#include "pcs_api/memory_byte_sink.h"
#include "pcs_api/memory_byte_source.h"
#include "pcs_api/internal/progress_byte_sink.h"
#include "pcs_api/internal/progress_byte_source.h"


namespace pcs_api {

TEST(ModelsTest, TestCPath) {
    CPath path(PCS_API_STRING_T("/foo//bar\u20AC/"));
    EXPECT_EQ(PCS_API_STRING_T("/foo/bar\u20AC"), path.path_name());
    EXPECT_EQ(PCS_API_STRING_T("/foo/bar%E2%82%AC"), path.GetUrlEncoded());
    EXPECT_EQ(PCS_API_STRING_T("bar\u20AC"), path.GetBaseName());
    EXPECT_EQ(CPath(PCS_API_STRING_T("/foo")), path.GetParent());
    EXPECT_EQ(path.Add(PCS_API_STRING_T("a,file...")),
              CPath(PCS_API_STRING_T("/foo/bar\u20AC/a,file...")));
    EXPECT_EQ(path.Add(PCS_API_STRING_T("/a,file...")),
              CPath(PCS_API_STRING_T("/foo/bar\u20AC/a,file...")));
    EXPECT_EQ(path.Add(PCS_API_STRING_T("a,file.../")),
              CPath(PCS_API_STRING_T("/foo/bar\u20AC/a,file...")));
    EXPECT_EQ(path.Add(PCS_API_STRING_T("/several//folders/he re/")),
        CPath(PCS_API_STRING_T("/foo/bar\u20AC/several/folders/he re")));
    EXPECT_FALSE(path.IsRoot());
    EXPECT_FALSE(path.GetParent().IsRoot());

    CPath root = path.GetParent().GetParent();
    EXPECT_TRUE(root.IsRoot());
    EXPECT_TRUE(root.GetParent().IsRoot());
    EXPECT_EQ(root, CPath(PCS_API_STRING_T("/")));
    EXPECT_EQ(root, CPath(PCS_API_STRING_T("")));
    EXPECT_EQ(root.GetBaseName(), PCS_API_STRING_T(""));

    EXPECT_EQ(0, root.Split().size());
    EXPECT_EQ(0, CPath(PCS_API_STRING_T("")).Split().size());
    std::vector<string_t> expected;
    expected.push_back(PCS_API_STRING_T("a"));
    EXPECT_EQ(expected, CPath(PCS_API_STRING_T("/a")).Split());
    expected.clear();
    expected.push_back(PCS_API_STRING_T("alpha"));
    expected.push_back(PCS_API_STRING_T("\"beta"));
    EXPECT_EQ(expected, CPath(PCS_API_STRING_T("/alpha/\"beta")).Split());
}

static CPath CreateCPath(string_t pathname) {
    return CPath(pathname);
}

TEST(ModelsTest, TestInvalidCPath) {
    std::vector<string_t> pathnames;
    pathnames.push_back(PCS_API_STRING_T("\\no anti-slash is allowed")),
    pathnames.push_back(PCS_API_STRING_T("This is an inv\u001Flid pathname !")),
    pathnames.push_back(PCS_API_STRING_T("This is an \t invalid pathname !")),
    pathnames.push_back(PCS_API_STRING_T("This/ is/an invalid pathname !")),
    pathnames.push_back(PCS_API_STRING_T("This/is /also invalid pathname !")),
    pathnames.push_back(PCS_API_STRING_T(" bad")),
    pathnames.push_back(PCS_API_STRING_T("bad "));
    for (string_t pathname : pathnames) {
        std::cout << "Checking CPath is invalid: "
                  << utility::conversions::to_utf8string(pathname) << std::endl;
        EXPECT_THROW(CreateCPath(pathname), std::invalid_argument)
            << "CPath creation should have failed for pathname='"
            << pathname << "'";
    }
}

TEST(ModelsTest, TestCPathUrlEncoded) {
    EXPECT_EQ(PCS_API_STRING_T("/a%20%2B%25b/c"),
              CPath(PCS_API_STRING_T("/a +%b/c")).GetUrlEncoded());
    EXPECT_EQ(PCS_API_STRING_T("/a%3Ab"),
              CPath(PCS_API_STRING_T("/a:b")).GetUrlEncoded());
    EXPECT_EQ(PCS_API_STRING_T("/%E2%82%AC"),
              CPath(PCS_API_STRING_T("/\u20AC")).GetUrlEncoded());
    EXPECT_EQ(PCS_API_STRING_T(
                "/%21%20%22%23%24%25%26%27%28%29%2A%2B%2C-./09%3A%3B%3C%3D%3E%3F%40AZ%5B%5D%5E_%60az%7B%7C%7D~"),  // NOLINT
              CPath(PCS_API_STRING_T(
                "/! \"#$%&'()*+,-./09:;<=>?@AZ[]^_`az{|}~")).GetUrlEncoded());
}

TEST(ModelsTest, TestDownloadRequestBytesRange) {
    std::shared_ptr<ByteSink> p_bs = std::make_shared<MemoryByteSink>();
    CDownloadRequest dr(CPath(PCS_API_STRING_T("/foo")), p_bs);

    // By default we have no range header:
    std::map<string_t, string_t> headers = dr.GetHttpHeaders();
    // no Range header:
    ASSERT_FALSE(headers.find(PCS_API_STRING_T("Range")) != headers.end());

    dr.SetRange(-1, 100);
    headers = dr.GetHttpHeaders();
    EXPECT_EQ(PCS_API_STRING_T("bytes=-100"),
              headers[PCS_API_STRING_T("Range")]);

    dr.SetRange(10, 100);
    headers = dr.GetHttpHeaders();
    EXPECT_EQ(PCS_API_STRING_T("bytes=10-109"),
              headers[PCS_API_STRING_T("Range")]);

    dr.SetRange(0, -1);
    headers = dr.GetHttpHeaders();
    EXPECT_EQ(PCS_API_STRING_T("bytes=0-"),
              headers[PCS_API_STRING_T("Range")]);

    dr.SetRange(100, -1);
    headers = dr.GetHttpHeaders();
    EXPECT_EQ(PCS_API_STRING_T("bytes=100-"),
              headers[PCS_API_STRING_T("Range")]);

    dr.SetRange(-1, -1);
    headers = dr.GetHttpHeaders();
    // no Range header:
    ASSERT_FALSE(headers.find(PCS_API_STRING_T("Range")) != headers.end());
}

TEST(ModelsTest, TestDownloadRequestProgressListener) {
    std::shared_ptr<ByteSink> p_bs = std::make_shared<MemoryByteSink>();
    CDownloadRequest dr(CPath(PCS_API_STRING_T("/foo")), p_bs);
    // Without progress listener we get the same byte sink:
    EXPECT_EQ(p_bs, dr.GetByteSink());  // check shared_ptr equality

    // Now if we decorate:
    std::shared_ptr<StdoutProgressListener> p_pl =
                                    std::make_shared<StdoutProgressListener>();
    dr.set_progress_listener(p_pl);
    std::shared_ptr<ByteSink> p_progress_bs = dr.GetByteSink();
    EXPECT_EQ(typeid(*p_progress_bs.get()), typeid(detail::ProgressByteSink));

    // Check that listener is notified:
    std::ostream *p_os = p_progress_bs->OpenStream();
    p_os->put(65);
    p_os->flush();
    EXPECT_EQ(1, p_pl->current());
    p_progress_bs->CloseStream();
}

TEST(ModelsTest, TestUploadRequestProgressListener) {
    char data[] = "content of byte source...";
    std::shared_ptr<ByteSource> p_bs = std::make_shared<MemoryByteSource>(data);
    CUploadRequest ur(CPath(PCS_API_STRING_T("/foo")), p_bs);
    // Without progress listener we get the same byte source:
    ASSERT_EQ(p_bs, ur.GetByteSource());

    // Now if we decorate:
    std::shared_ptr<StdoutProgressListener> p_pl =
                                    std::make_shared<StdoutProgressListener>();
    ur.set_progress_listener(p_pl);
    std::shared_ptr<ByteSource> p_progress_bs = ur.GetByteSource();
    ASSERT_EQ(typeid(*p_progress_bs.get()), typeid(detail::ProgressByteSource));

    // Check that listener is notified:
    std::unique_ptr<std::istream> p_is = p_progress_bs->OpenStream();
    p_is->get();
    ASSERT_GT(p_pl->current(), 0);
}


}  // namespace pcs_api

