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

#ifndef INCLUDE_PCS_API_INTERNAL_UTILITIES_H_
#define INCLUDE_PCS_API_INTERNAL_UTILITIES_H_

#include <string>
#include <stdexcept>
#include <system_error>

#include "boost/date_time/posix_time/ptime.hpp"

#include "pcs_api/types.h"

namespace pcs_api {

namespace utilities {

/**
 * \brief Generate a random number in [0;1) interval.
 * 
 * @return (pseudo) random value
 */
double Random();

/**
 * \brief Generate a random string of given length.
 *
 * @param length
 * @return string composed of letters and digits only
 */
std::string GenerateRandomString(size_t length);

/**
 * \brief Abbreviate a string if too long.
 *
 * This is for development purposes only; string may be cut in the middle
 * of a UTF-8 sequence.
 *
 * @param source the string to abbreviate
 * @param max_len 
 * @return source if shorter than max_len, or abbreviated + "..." string
 */
std::string Abbreviate(const std::string& source, size_t max_len);

/**
 * \brief Escape a string for XML concatenation.
 *
 * @param source the string to escape
 * @return source with reserved characters replaced by entities:
 *         &gt; &lt; &apos; &quot;
*/
std::string EscapeXml(const std::string& source);

/**
 * \brief Convert a posix date_time to number of seconds since 1970
 */
int64_t DateTimeToTime_t(const ::boost::posix_time::ptime& pt);

/**
* \brief Convert a posix date_time to number of milliseconds since 1970
*/
int64_t DateTimeToTime_t_ms(const ::boost::posix_time::ptime& pt);


}  // namespace utilities

}  // namespace pcs_api


#endif  // INCLUDE_PCS_API_INTERNAL_UTILITIES_H_

