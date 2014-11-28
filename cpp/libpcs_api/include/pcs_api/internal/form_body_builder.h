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

#ifndef INCLUDE_PCS_API_INTERNAL_FORM_BODY_BUILDER_H_
#define INCLUDE_PCS_API_INTERNAL_FORM_BODY_BUILDER_H_

#include <vector>
#include <utility>

namespace pcs_api {

/**
 * \brief A utility class for generating the body of a form POST.
 */
class FormBodyBuilder {
 public:
    void AddParameter(string_t name, string_t value);
    string_t ContentType() const;
    const std::vector<unsigned char> Build() const;
 private:
    std::vector<std::pair<string_t, string_t>> parameters_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_FORM_BODY_BUILDER_H_

