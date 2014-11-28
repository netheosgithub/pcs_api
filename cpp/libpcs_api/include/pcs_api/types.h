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

#ifndef INCLUDE_PCS_API_TYPES_H_
#define INCLUDE_PCS_API_TYPES_H_

/**
 * libpcs_api basic types definitions
 */

#include <string>

namespace pcs_api {
// General definitions

// See cpprest basic_types.h:
// strings are wide (UTF-16) on Windows, and narrow (UTF-8) on other platforms.
// See
// http://casablanca.codeplex.com/wikipage?title=Platform-Independent%20Strings
// We redefine these here to avoid including any cpprest header

#if defined _WIN32 || defined _WIN64
    typedef wchar_t char_t;
    typedef std::wstring string_t;
    #define PCS_API_STRING_T(x) L ## x
    // MSVC doesn't support this yet
    #define _noexcept

#else
    typedef char char_t;
    typedef std::string string_t;
    #define PCS_API_STRING_T(x) x
    #define _noexcept noexcept

#endif

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_TYPES_H_

