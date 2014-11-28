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

#include "pcs_api/c_quota.h"

namespace pcs_api {

CQuota::CQuota(int64_t bytes_used, int64_t bytes_allowed) :
    bytes_used_(bytes_used),
    bytes_allowed_(bytes_allowed) {
}

float CQuota::GetPercentUsed() const {
    if (bytes_used_ >= 0 && bytes_allowed_ > 0) {
        return bytes_used_ * 100.f / bytes_allowed_;
    } else {
        return -1.f;
    }
}

std::ostream& operator<<(std::ostream& strm, const CQuota& q) {
    return strm << "CQuota(used=" << q.bytes_used()
                << ", allowed=" << q.bytes_allowed()
                << ", %Used=" << q.GetPercentUsed() << "%)";
}

}  // namespace pcs_api
