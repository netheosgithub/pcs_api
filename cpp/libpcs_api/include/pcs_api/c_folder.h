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

#ifndef INCLUDE_PCS_API_C_FOLDER_H_
#define INCLUDE_PCS_API_C_FOLDER_H_

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "pcs_api/c_file.h"


namespace pcs_api {

/**
 * \brief Object for representation of a remote folder.
 */
class CFolder : public CFile {
 public:
    CFolder(CPath path, ::boost::posix_time::ptime modif_date);
 private:
    void DoToString(std::ostream *) const override;
    bool IsBlobType() const override;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_FOLDER_H_


