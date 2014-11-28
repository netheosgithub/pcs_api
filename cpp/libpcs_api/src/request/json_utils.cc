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

#include "boost/lexical_cast.hpp"

#include "pcs_api/internal/json_utils.h"

namespace pcs_api {

template<> const bool CastJsonValueAsT(const web::json::value& value) {
    return value.as_bool();
}

template<> const string_t CastJsonValueAsT(const web::json::value& value) {
    return value.as_string();
}

#if defined _WIN32 || defined _WIN64
template<> const std::string CastJsonValueAsT(const web::json::value& value) {
    return utility::conversions::to_utf8string(value.as_string());
}
#endif

template<> const int32_t CastJsonValueAsT(const web::json::value& value) {
    if (value.is_number()) {
        // this should be the usual path:
        return value.as_number().to_int32();
    } else {
        // we accept to parse strings:
        return boost::lexical_cast<int32_t>(value.as_string());
    }
}

template<> const int64_t CastJsonValueAsT(const web::json::value& value) {
    if (value.is_number()) {
        // this should be the usual path:
        return value.as_number().to_int64();
    } else {
        // we accept to parse strings:
        return boost::lexical_cast<int64_t>(value.as_string());
    }
}

}  // namespace pcs_api
