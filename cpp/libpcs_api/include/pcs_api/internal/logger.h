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

#ifndef INCLUDE_PCS_API_INTERNAL_LOGGER_H_
#define INCLUDE_PCS_API_INTERNAL_LOGGER_H_

/**
 * \brief pcs_api logging macro definitions.
 */

#include "boost/log/attributes/named_scope.hpp"
#include "boost/log/trivial.hpp"

#define LOG_TRACE \
  BOOST_LOG_TRIVIAL(trace) << "[" << __FILE__ << ":" << __LINE__ << "] "
#define LOG_DEBUG \
  BOOST_LOG_TRIVIAL(debug) << "[" << __FILE__ << ":" << __LINE__ << "] "
#define LOG_INFO \
  BOOST_LOG_TRIVIAL(info) << "[" << __FILE__ << ":" << __LINE__ << "] "
#define LOG_WARN \
  BOOST_LOG_TRIVIAL(warning) << "[" << __FILE__ << ":" << __LINE__ << "] "
#define LOG_ERROR \
  BOOST_LOG_TRIVIAL(error) << "[" << __FILE__ << ":" << __LINE__ << "] "

#endif  // INCLUDE_PCS_API_INTERNAL_LOGGER_H_

