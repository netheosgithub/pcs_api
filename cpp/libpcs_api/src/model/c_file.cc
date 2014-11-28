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

#include "cpprest/asyncrt_utils.h"

#include "pcs_api/c_file.h"

namespace pcs_api {

CFile::CFile(CPath path, ::boost::posix_time::ptime modif_date) :
    path_(path), modification_date_(modif_date) {
}

CFile::~CFile() {
}

bool CFile::IsFolder() const {
    return !IsBlobType();
}

bool CFile::IsBlob() const {
    return IsBlobType();
}

std::ostream& operator<<(std::ostream& strm, const CFile &cfile) {
    cfile.DoToString(&strm);
    return strm;
}

std::string CFile::ToString() const {
    std::ostringstream tmp;
    tmp << *this;
    return tmp.str();
}

}  // namespace pcs_api
