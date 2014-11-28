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

#include <ostream>  // NOLINT(readability/streams)
#include <random>

#include "pcs_api/internal/utilities.h"
#include "pcs_api/internal/logger.h"


namespace pcs_api {

namespace utilities {

/**
 * \brief shared random device ; accessed by multiple threads, should not hurt.
 * 
 * hopefully non deterministic ?
 */
static std::random_device *p_shared_random_device;

// We use a dummy object constructor to create random_device
// This will never be destroyed.
// See google style guide, Static_and_Global_Variables
namespace details {
class random_initializer {
 public:
    random_initializer() {
        p_shared_random_device = new std::random_device();
    }
};
static const random_initializer initializer;
}  // namespace details

double Random() {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(*p_shared_random_device);
}

std::string GenerateRandomString(size_t length) {
    std::string ret;
    ret.resize(length);
    // A-Z, a-z, 0-9:
    std::uniform_int_distribution<int> distribution(0, 26+26+10-1);
    for (unsigned int i = 0 ; i < length ; ++i) {
        int k = distribution(*p_shared_random_device);
        char c;
        if (k < 26) {
            c = 'A' + k;
        } else if (k < 26+26) {
            c = 'a' + k - 26;
        } else {
            c = '0' + k - 26 - 26;
        }
        ret[i] = c;
    }
    return ret;
}

std::string Abbreviate(const std::string& source, size_t max_len) {
    if (source.length() <= max_len) {
        return source;
    }
    // may cut in a utf-8 sequence:
    return source.substr(0, max_len) + "...";
}

std::string EscapeXml(const std::string& s) {
    std::string ret;
    ret.reserve(s.length() * 2);
    for (char c : s) {
        if (c == '<') {
            ret.append("&lt;");
        } else if (c == '>') {
            ret.append("&gt;");
        } else if (c == '&') {
            ret.append("&amp;");
        } else if (c == '\'') {
            ret.append("&apos;");
        } else if (c == '\"') {
            ret.append("&quot;");
        } else {
            ret += c;
        }
    }
    return ret;
}

static boost::posix_time::time_duration DiffDateTimeToEpoch(
                                        const boost::posix_time::ptime& pt) {
    static boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
    return boost::posix_time::time_duration(pt - epoch);
}

int64_t DateTimeToTime_t(const boost::posix_time::ptime& pt) {
    boost::posix_time::time_duration diff = DiffDateTimeToEpoch(pt);
    return diff.total_seconds();
}

int64_t DateTimeToTime_t_ms(const boost::posix_time::ptime& pt) {
    boost::posix_time::time_duration diff = DiffDateTimeToEpoch(pt);
    return diff.total_milliseconds();
}

}  // namespace utilities

}  // namespace pcs_api

