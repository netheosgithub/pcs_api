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

#include <utility>
#include <bitset>

#include "boost/algorithm/string.hpp"
#include "boost/format.hpp"

#include "cpprest/asyncrt_utils.h"
#include "cpprest/base_uri.h"

#include "pcs_api/types.h"
#include "pcs_api/internal/uri_utils.h"

namespace pcs_api {

static const std::bitset<256> BuildSafeChars() {
    std::bitset<256> safe_chars;
    for (int i = 'a'; i <= 'z'; i++) {
        safe_chars.set(i);
    }
    for (int i = 'A'; i <= 'Z'; i++) {
        safe_chars.set(i);
    }
    // numeric characters
    for (int i = '0'; i <= '9'; i++) {
        safe_chars.set(i);
    }
    safe_chars.set('-');
    safe_chars.set('.');
    safe_chars.set('_');
    safe_chars.set('~');
    return safe_chars;
}

static const std::bitset<256> kSafeChars = BuildSafeChars();

std::string UriUtils::ShortenUri(const web::uri& uri) {
    // Note: cpprest is wrong here: scheme is considered as part of authority
    string_t short_uri = uri.authority().to_string();
    if (short_uri.at(short_uri.length()-1) == '/') {
        // remove trailing slash, also part of path:
        short_uri.erase(short_uri.length()- 1);
    }
    short_uri += uri.path();
    return utility::conversions::to_utf8string(short_uri);
}

static std::string Encode(const string_t& content,
                          bool param) {
    std::string ret;
    // convert to UTF-8 bytes:
    std::string utf8_string = utility::conversions::to_utf8string(content);

    // Now escape invalid chars:
    for (auto it = utf8_string.begin() ; it != utf8_string.end() ; ++it) {
        char c = *it;
        int i = (unsigned char)c;  // 0 to 255
        // safe ?
        if (kSafeChars[i]) {
            ret += c;
        } else if (c == ' ') {
            if (!param) {
                ret += "%20";
            } else {
                ret += '+';
            }
        } else if (c == '/') {
            if (!param) {
                ret += c;  // slashes are not encoded in paths
            } else {
                ret += "%2F";
            }
        } else {
            ret += (boost::format("%%%|02X|") % i).str();
        }
    }
    return ret;
}

std::string UriUtils::EscapeUriPath(const string_t& unencoded_path) {
    return Encode(unencoded_path, false);
}

std::string UriUtils::EscapeQueryParameter(const string_t& unencoded_param) {
    return Encode(unencoded_param, true);
}

string_t UriUtils::UnescapeQueryParameter(const string_t& encoded_param) {
    // + are spaces in query parameters,
    // this is not handled by web::uri::decode():
    string_t encoded_spaces_param = encoded_param;
    for (auto it = encoded_spaces_param.begin();
         it != encoded_spaces_param.end();
         ++it) {
        if (*it == U('+')) {
            *it = U(' ');
        }
    }
    return web::uri::decode(encoded_spaces_param);
}

string_t UriUtils::GetQueryParameter(const web::uri& uri,
                                     const string_t& param_name) {
    std::map<string_t, string_t> params = ParseQueryParameters(uri.query());
    auto it = params.find(param_name);
    if (it == params.end()) {
        return string_t();
    }
    return it->second;
}

std::map<string_t, string_t> UriUtils::ParseQueryParameters(
                                                    const string_t& query) {
    std::map<string_t, string_t> ret;
    if (query.empty()) {
        return ret;
    }
    std::vector<string_t> pairs;
    boost::algorithm::split(pairs, query, boost::is_any_of(U("&")));
    for (auto it = pairs.begin() ; it != pairs.end() ; ++it) {
        string_t pair = *it;
        size_t idx = pair.find_first_of(U("="));
        string_t name, value;
        if (idx != std::string::npos && idx != 0) {
            // name=value
            name = pair.substr(0, idx);
            value = UnescapeQueryParameter(pair.substr(idx + 1));
            ret[name] = value;
        }
    }

    return ret;
    }


}  // namespace pcs_api

