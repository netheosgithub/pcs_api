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

#ifndef INCLUDE_PCS_API_C_FILE_H_
#define INCLUDE_PCS_API_C_FILE_H_

#include <string>

#include "boost/date_time/posix_time/posix_time_types.hpp"

#include "pcs_api/c_path.h"


namespace pcs_api {

/**
 * \brief Base class for representation of a remote file (folder or blob).
 *
 * For developement, CFile objects can be dumped polymorphically to a stream.
 */
class CFile {
 public:
    CPath path() const {
        return path_;
    }
    ::boost::posix_time::ptime modification_date() const {
        return modification_date_;
    }
    bool IsFolder() const;
    bool IsBlob() const;
    std::string ToString() const;
    virtual ~CFile();
    friend std::ostream& operator<<(std::ostream&, const CFile&);

 protected:
    CFile(CPath cpath, ::boost::posix_time::ptime modif_date);
    CPath path_;
    ::boost::posix_time::ptime modification_date_;
 private:
    virtual bool IsBlobType() const = 0;
    virtual void DoToString(std::ostream *) const = 0;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_FILE_H_


