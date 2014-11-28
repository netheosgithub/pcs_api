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

#include "gtest/gtest.h"

#include "pcs_api/types.h"
#include "pcs_api/internal/uri_utils.h"
#include "pcs_api/internal/utilities.h"

namespace pcs_api {

TEST(UriUtilsTest, TestGetQueryParameter) {
    string_t url = U("http://www.host.net/path/test?code=edc&test=1212");
    web::uri uri(url);

    EXPECT_EQ(U("edc"), UriUtils::GetQueryParameter(uri, U("code")));
    EXPECT_TRUE(UriUtils::GetQueryParameter(uri, U("cod") ).empty());

    uri = web::uri(U("http://www.host.net/path/test?code=edc+1&empty=&q=with+an+%26&test=%22foo+bar%E2%82%AC%22"));  // NOLINT
    EXPECT_EQ(U("edc 1"), UriUtils::GetQueryParameter(uri, U("code")));
    EXPECT_EQ(U(""), UriUtils::GetQueryParameter(uri, U("empty")));
    EXPECT_EQ(U("\"foo bar\u20AC\""),
              UriUtils::GetQueryParameter(uri, U("test")));
    EXPECT_EQ(U("with an &"), UriUtils::GetQueryParameter(uri, U("q")));
}

TEST(UriUtilsTest, TestParseQueryParameters) {
    web::uri uri(U("https://localhost/?b=&c=%22"));
    std::map<string_t, string_t> params =
                                UriUtils::ParseQueryParameters(uri.query());
    EXPECT_EQ(U(""), params[U("b")]);
    EXPECT_EQ(U("\""), params[U("c")]);

    params = UriUtils::ParseQueryParameters(U(""));
    EXPECT_EQ(0, params.size());
    params = UriUtils::ParseQueryParameters(U("&"));
    EXPECT_EQ(0, params.size());
    params = UriUtils::ParseQueryParameters(U("&&"));
    EXPECT_EQ(0, params.size());
}

// URIUtils::EscapeUriPath is tested in model: CPath.GetUrlEncoded()

TEST(UriUtilsTest, TestEscapeQueryParameter) {
    EXPECT_EQ("", UriUtils::EscapeQueryParameter(U("")));
    EXPECT_EQ("value1", UriUtils::EscapeQueryParameter(U("value1")));
    EXPECT_EQ("par%C3%A0m2", UriUtils::EscapeQueryParameter(U("par\u00e0m2")));
    // and back:
    EXPECT_EQ(U("par\u00e0m2"),
              UriUtils::UnescapeQueryParameter(U("par%C3%A0m2")));

    EXPECT_EQ("va+lu%E2%82%AC2+",
              UriUtils::EscapeQueryParameter(U("va lu\u20AC2 ")));
    // and back:
    EXPECT_EQ(U("va lu\u20AC2 "),
              UriUtils::UnescapeQueryParameter(U("va+lu%E2%82%AC2+")));
}

TEST(UtilitiesTest, TestEscapeXml) {
    EXPECT_EQ("", utilities::EscapeXml(""));
    EXPECT_EQ("value1:~ ", utilities::EscapeXml("value1:~ "));
    EXPECT_EQ("val&quot;&amp;ue1", utilities::EscapeXml("val\"&ue1"));
    EXPECT_EQ("&apos;&amp;amp;&lt;&gt;&lt;",
              utilities::EscapeXml("\'&amp;<><"));
}

}  // namespace pcs_api
