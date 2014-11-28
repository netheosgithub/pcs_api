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

#ifndef INCLUDE_PCS_API_C_BLOB_H_
#define INCLUDE_PCS_API_C_BLOB_H_

#include <cstdint>

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "pcs_api/c_file.h"

namespace pcs_api {

/**
 * \brief Local object representing a remote regular file.
 */
class CBlob : public CFile {
 public:
    CBlob(CPath path,
          int64_t length,
          string_t content_type,
          ::boost::posix_time::ptime modif_date);
    int64_t length() const {
        return length_;
    }
    string_t content_type() const {
        return content_type_;
    }

 private:
    void DoToString(std::ostream *) const override;
    bool IsBlobType() const override;
    int64_t length_;
    string_t content_type_;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_BLOB_H_


