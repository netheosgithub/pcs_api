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

#ifndef INCLUDE_PCS_API_INTERNAL_URI_UTILS_H_
#define INCLUDE_PCS_API_INTERNAL_URI_UTILS_H_

#include <string>
#include <map>

#include "cpprest/uri.h"

#include "pcs_api/types.h"

namespace pcs_api {

/**
 * \brief Some utility static methods for handling uri.
 */
class UriUtils {
 public:
    static std::string ShortenUri(const web::uri& uri);
    static std::string EscapeUriPath(const string_t& unencoded_path);
    static std::string EscapeQueryParameter(const string_t& unencoded_param);
    static string_t UnescapeQueryParameter(const string_t& encoded_param);
    static std::map<string_t, string_t> ParseQueryParameters(
                                                        const string_t& query);
    static string_t GetQueryParameter(const web::uri& uri,
                                      const string_t& param_name);
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_URI_UTILS_H_

