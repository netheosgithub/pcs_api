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

#include "cpprest/http_helpers.h"

#include "pcs_api/types.h"
#include "pcs_api/internal/uri_utils.h"
#include "pcs_api/internal/form_body_builder.h"

namespace pcs_api {

void FormBodyBuilder::AddParameter(string_t name, string_t value) {
    parameters_.push_back(std::pair<string_t, string_t>(name, value));
}

string_t FormBodyBuilder::ContentType() const {
    return web::http::details::mime_types::application_x_www_form_urlencoded;
}

const std::vector<unsigned char> FormBodyBuilder::Build() const {
    std::string ret;
    for (auto it = parameters_.begin() ; it != parameters_.end() ; ++it) {
        auto encoded_name = UriUtils::EscapeQueryParameter(it->first);
        auto encoded_value = UriUtils::EscapeQueryParameter(it->second);
        if (ret.length() > 0) {
            ret += '&';
        }
        ret += encoded_name;
        ret += '=';
        ret += encoded_value;
    }
    return std::vector<unsigned char>(ret.cbegin(), ret.cend());
}


}  // namespace pcs_api
