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

#include "boost/algorithm/string.hpp"

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/model.h"
#include "pcs_api/internal/uri_utils.h"

namespace pcs_api {

static const char_t FORBIDDEN_CHAR = PCS_API_STRING_T('\\');

CPath::CPath(const string_t& path_name)
    : path_name_(CPath::CheckNormalize(path_name)) {
}

std::string CPath::path_name_utf8() const {
    return utility::conversions::to_utf8string(path_name_);
}

string_t CPath::GetUrlEncoded() const {
    std::string escaped_utf8 = UriUtils::EscapeUriPath(path_name_);
    // A url path IS a 8-bits string
    // However, cpp rest SDK always expects wide strings under windows,
    // so we "enlarge" the narrow string to convert
    // (no "conversion" is actually involved since all bytes are ascii):
    // (this is a no-op if string_t is narrow)
    string_t ret = utility::conversions::to_string_t(escaped_utf8);
    return ret;
}

string_t CPath::GetBaseName() const {
    size_t pos = path_name_.find_last_of(PCS_API_STRING_T("/"));  // never npos
    return path_name_.substr(pos+1);
}

bool CPath::IsRoot() const {
    return path_name_ == PCS_API_STRING_T("/");
}

std::vector<string_t> CPath::Split() const {
    std::vector<string_t> segments;
    if (IsRoot()) {
        return segments;  // empty
    }
    auto without_slash = path_name_.substr(1);
    ::boost::algorithm::split(segments,
                              without_slash,
                              boost::is_any_of(PCS_API_STRING_T("/")));
    return segments;
}

CPath CPath::GetParent() const {
    // never npos:
    size_t last_index = path_name_.find_last_of(PCS_API_STRING_T("/"));
    if (last_index == 0) {
        return CPath(PCS_API_STRING_T("/"));
    }
    return CPath(path_name_.substr(0, last_index));
}

CPath CPath::Add(string_t name) const {
    return CPath(path_name_ + PCS_API_STRING_T("/") + name);
}

string_t CPath::CheckNormalize(const string_t& path_name) {
    Check(path_name);
    return Normalize(path_name);
}

void CPath::Check(const string_t& path_name) {
    // We keep current encoding for checks
    // works for UTF-8 as long as invalid chars have codepoints < 128
    for (const char_t& c : path_name) {
        if ((unsigned)c < 32 || FORBIDDEN_CHAR == c) {
            string_t msg = PCS_API_STRING_T("Pathname contains invalid char '");
            msg += c;
            msg += PCS_API_STRING_T("': ") + path_name;
            BOOST_THROW_EXCEPTION(std::invalid_argument(
                                utility::conversions::to_utf8string(msg)));
        }
    }
    // Check each segment:
    std::vector<string_t> segments;
    ::boost::algorithm::split(segments,
                              path_name,
                              boost::is_any_of(PCS_API_STRING_T("/")));
    for (auto it = segments.begin() ; it != segments.end() ; ++it) {
        string_t segment = *it;
        if (boost::algorithm::trim_copy(segment) != segment) {
            string_t msg = PCS_API_STRING_T(
                "Pathname contains leading or trailing spaces: ");
            msg += path_name;
            BOOST_THROW_EXCEPTION(std::invalid_argument(
                                    utility::conversions::to_utf8string(msg)));
        }
    }
}

string_t CPath::Normalize(string_t path_name) {
    char_t slash = PCS_API_STRING_T('/');
    // remove duplicated slashes:
    path_name.erase(std::unique(path_name.begin(), path_name.end(),
                                [=](char_t a, char_t b) {
                                    return a == slash && b == slash; }),
                    path_name.end());
    // remove trailing slash if present:
    size_t last = path_name.find_last_of(slash);
    if (last != std::string::npos && last == path_name.size()-1) {
        path_name.resize(last);
    }
    // Add a leading slash if missing:
    if (path_name.size() == 0 || path_name[0] != slash) {
        path_name.insert(0, 1, slash);
    }
    return path_name;
}

std::ostream& operator<<(std::ostream &strm, const CPath &path) {
  return strm << "CPath(" << path.path_name_utf8() << ")";
}

bool CPath::operator==(const CPath& other) const {
    return path_name_ == other.path_name_;
}

bool CPath::operator<(const CPath& other) const {
    return path_name_ < other.path_name_;
}

bool CPath::operator<=(const CPath& other) const {
    return path_name_ <= other.path_name_;
}

bool CPath::operator>(const CPath& other) const {
    return path_name_ > other.path_name_;
}

bool CPath::operator>=(const CPath& other) const {
    return path_name_ >= other.path_name_;
}

bool CPath::operator!=(const CPath& other) const {
    return path_name_ != other.path_name_;
}


}  // namespace pcs_api


