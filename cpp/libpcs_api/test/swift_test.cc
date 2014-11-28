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

#include "cpprest/json.h"
#include "cpprest/asyncrt_utils.h"

#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/providers/swift_client.h"
#include "pcs_api/internal/logger.h"

#include "misc_test_utils.h"

namespace pcs_api {

static void CheckParseLastModified(const std::string& last_modified,
                                   const int64_t expected_time_t_ms) {
    web::json::value val = web::json::value::object();
    val.as_object()[U("last_modified")] = web::json::value::string(
                            utility::conversions::to_string_t(last_modified));
    boost::posix_time::ptime pt = swift_details::ParseLastModified(val);
    LOG_INFO << "last_modified=" << last_modified << "   Parsed ptime=" << pt;
    int64_t pt_as_time_t_ms = utilities::DateTimeToTime_t_ms(pt);
    EXPECT_EQ(expected_time_t_ms, pt_as_time_t_ms);
    int64_t pt_as_time_t = utilities::DateTimeToTime_t(pt);
    EXPECT_EQ(expected_time_t_ms / 1000, pt_as_time_t);
}

TEST(SwiftTest, TestParseLastModified) {
    CheckParseLastModified("2014-01-15T16:37:43.427570", 1389803863427);
    CheckParseLastModified("2014-01-15T16:37:43.427", 1389803863427);
    CheckParseLastModified("2014-01-15T16:37:43.42", 1389803863420);
    CheckParseLastModified("2014-01-15T16:37:43.", 1389803863000);
    // timezone is ignored by parser:
    CheckParseLastModified("2014-01-15T16:37:43.+0000", 1389803863000);
    CheckParseLastModified("2014-01-15T16:37:43", 1389803863000);
}

TEST(SwiftTest, TestParseBadLastModified) {
    boost::posix_time::ptime nadt = boost::posix_time::not_a_date_time;

    web::json::value val = web::json::value::object();
    // nothing in json:
    boost::posix_time::ptime pt = swift_details::ParseLastModified(val);
    EXPECT_EQ(nadt, pt);

    val.as_object()[U("last_modified")] = web::json::value::string(
                                    utility::conversions::to_string_t("burp"));
    pt = swift_details::ParseLastModified(val);
    EXPECT_EQ(nadt, pt);
}

static void CheckParseTimestamp(const std::string& timestamp,
                                const int64_t expected_time_t_ms) {
    web::http::http_headers headers;
    headers.add(U("X-Timestamp"), utility::conversions::to_string_t(timestamp));
    boost::posix_time::ptime pt = swift_details::ParseTimestamp(headers);
    LOG_INFO << "timestamp=" << timestamp << "   Parsed ptime=" << pt;
    int64_t pt_as_time_t_ms = utilities::DateTimeToTime_t_ms(pt);
    EXPECT_EQ(expected_time_t_ms, pt_as_time_t_ms);
}

TEST(SwiftTest, TestParseTimestamp) {
    CheckParseTimestamp("1408550324.34246", 1408550324342);
    CheckParseTimestamp("1408550324.342", 1408550324342);
    CheckParseTimestamp("1408550324.34", 1408550324340);
    CheckParseTimestamp("1408550324.3", 1408550324300);
    CheckParseTimestamp("1408550324.", 1408550324000);
    CheckParseTimestamp("1408550324", 1408550324000);
}

TEST(SwiftTest, TestParseMissingTimestamp) {
    boost::posix_time::ptime nadt = boost::posix_time::not_a_date_time;

    web::http::http_headers headers;
    // no header:
    boost::posix_time::ptime pt = swift_details::ParseTimestamp(headers);
    EXPECT_EQ(nadt, pt);
}

}  // namespace pcs_api

