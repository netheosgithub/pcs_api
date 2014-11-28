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

#ifndef INCLUDE_PCS_API_C_QUOTA_H_
#define INCLUDE_PCS_API_C_QUOTA_H_

#include <cstdint>
#include <iostream>  // NOLINT(readability/streams)


namespace pcs_api {

/**
 * \brief Object storing storage usage information: used and available space
 *        (in bytes).
 */
class CQuota {
 public:
    CQuota(int64_t bytes_used, int64_t bytes_allowed);
    int64_t bytes_used() const {
        return bytes_used_;
    }
    int64_t bytes_allowed() const {
        return bytes_allowed_;
    }
    float GetPercentUsed() const;
    friend std::ostream& operator<<(std::ostream&, const CQuota&);

 private:
    int64_t bytes_used_;
    int64_t bytes_allowed_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_QUOTA_H_


