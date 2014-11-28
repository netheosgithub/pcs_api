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

#ifndef INCLUDE_PCS_API_C_PATH_H_
#define INCLUDE_PCS_API_C_PATH_H_

#include <string>
#include <vector>

#include "pcs_api/types.h"

namespace pcs_api {

/**
 * \brief Class holding a remote object path.
 *
 * A CPath is always absolute and always uses forward slash separators.
 * A CPath /foo/bar is composed of several segments: foo and bar.
 * Anti-slash are forbidden, so are spaces at the beginning or end of path
 * segments.
 */
class CPath {
 public:
    explicit CPath(const string_t& path_name);

    string_t path_name() const {
        return path_name_;
    }
    std::string path_name_utf8() const;
    string_t GetUrlEncoded() const;
    string_t GetBaseName() const;
    bool IsRoot() const;
    std::vector<string_t> Split() const;
    CPath GetParent() const;
    CPath Add(string_t name) const;
    friend std::ostream& operator<<(std::ostream&, const CPath&);
    bool operator==(const CPath& other) const;
    bool operator<(const CPath& other) const;  // for using CPath as keys
    bool operator<=(const CPath& other) const;
    bool operator>(const CPath& other) const;
    bool operator>=(const CPath& other) const;
    bool operator!=(const CPath& other) const;

 private:
    string_t path_name_;
    static string_t CheckNormalize(const string_t& path_name);
    static void Check(const string_t& path_name);
    static string_t Normalize(string_t path_name);
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_PATH_H_


