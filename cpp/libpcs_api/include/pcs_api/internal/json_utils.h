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

#ifndef INCLUDE_PCS_API_INTERNAL_JSON_UTILS_H_
#define INCLUDE_PCS_API_INTERNAL_JSON_UTILS_H_

/**
 * \brief Some utility functions related to json
 */

#include <string>

#include "cpprest/json.h"

#include "pcs_api/types.h"

namespace pcs_api {

template<class T>
const T CastJsonValueAsT(const web::json::value& value);
template<> const bool CastJsonValueAsT(const web::json::value& value);
template<> const string_t CastJsonValueAsT(const web::json::value& value);
#if defined _WIN32 || defined _WIN64
template<> const std::string CastJsonValueAsT(const web::json::value& value);
#endif
template<> const int32_t CastJsonValueAsT(const web::json::value& value);
template<> const int64_t CastJsonValueAsT(const web::json::value& value);

template <class T> T JsonForKey(const web::json::value& value,
                                string_t key,
                                T default_value) {
    const web::json::object& obj = value.as_object();
    auto it = obj.find(key);
    if (it == obj.end()) {
        return default_value;
    }
    return CastJsonValueAsT<T>(it->second);
}

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_JSON_UTILS_H_

