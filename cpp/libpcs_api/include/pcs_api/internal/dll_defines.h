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

#ifndef INCLUDE_PCS_API_INTERNAL_DLL_DEFINES_H_
#define INCLUDE_PCS_API_INTERNAL_DLL_DEFINES_H_

/* Cmake will define pcs_api_EXPORTS on Windows when it
configures to build a shared library. If you are going to use
another build system on windows or create the visual studio
projects by hand you need to define pcs_api_EXPORTS when
building a DLL on windows.
*/
// We are using the Visual Studio Compiler and building Shared libraries

// Unused header file: for now libpcs_api can not be a DLL,
// only a static library.

#if defined (_WIN32)
#if defined(pcs_api_EXPORTS)
#define  PCS_API_EXPORT __declspec(dllexport)
#else
#define  PCS_API_EXPORT __declspec(dllimport)
#endif /* pcs_api_EXPORTS */
#else /* defined (_WIN32) */
#define PCS_API_EXPORT
#endif

#endif  // INCLUDE_PCS_API_INTERNAL_DLL_DEFINES_H_
