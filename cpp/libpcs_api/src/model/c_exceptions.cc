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

#include <exception>

#include "boost/algorithm/string.hpp"
#include "boost/format.hpp"
#include "boost/exception/all.hpp"

#include "cpprest/http_msg.h"

#include "pcs_api/c_exceptions.h"

namespace pcs_api {

CStorageException::CStorageException() {
}

CStorageException::CStorageException(const std::string& message) :
    message_(message) {
}

CStorageException::CStorageException(const std::string& message,
                                     std::exception_ptr p_cause) :
    message_(message), p_cause_(p_cause) {
}

std::string CStorageException::ToString() const {
    std::ostringstream tmp;
    tmp << "CStorageException: " << what();
    return tmp.str();
}



CHttpException::CHttpException(const std::string& message,
                               int status,
                               const std::string& reason,
                               const std::string& method,
                               const std::string& url) :
        CStorageException(message),
        status_(status),
        reason_(reason),
        request_method_(method),
        url_(url) {
}

static std::string CHttpExceptionToString(const CHttpException& che,
                                          std::string class_name) {
    std::ostringstream tmp;
    tmp << boost::format("%s: %s %s [%i/%s] %s") %
            class_name % che.request_method() % che.url()
            % che.status() % che.reason() % che.what();
    return tmp.str();
}

std::string CHttpException::ToString() const {
    return CHttpExceptionToString(*this, "CHttpException");
}


CAuthenticationException::CAuthenticationException(const std::string& message,
                                                   int status,
                                                   const std::string& reason,
                                                   const std::string& method,
                                                   const std::string& url)
    : CHttpException(message, status, reason, method, url) {
}

std::string CAuthenticationException::ToString() const {
    return CHttpExceptionToString(*this, "CAuthenticationException");
}


CFileNotFoundException::CFileNotFoundException(const std::string& message,
                                               const CPath& path)
    : CStorageException(message),
    path_(path) {
}

std::string CFileNotFoundException::ToString() const {
    std::ostringstream tmp;
    tmp << "CFileNotFoundException: " << what() << " " << path();
    return tmp.str();
}



CInvalidFileTypeException::CInvalidFileTypeException(const CPath& path,
                                                     bool blob_expected)
    : path_(path),
      blob_expected_(blob_expected) {
}

std::string CInvalidFileTypeException::ToString() const {
    std::ostringstream tmp;
    tmp << "CInvalidFileTypeException: " << what() << " " << path()
            << ": expected " << (blob_expected() ? "blob" : "folder");
    return tmp.str();
}


CRetriableException::CRetriableException(std::exception_ptr p_cause,
                                         std::chrono::milliseconds delay) :
        CStorageException("Wrapped to be retried", p_cause),
        delay_(delay) {
}

std::string CRetriableException::ToString() const {
    std::ostringstream tmp;
    tmp << "CRetriableException: " << what();
    return tmp.str();
}


// Below are some utility functions to pretty log exceptions

static void TrimTrailingNulls(std::string *p_string) {
    for (auto it = p_string->begin(); it != p_string->end(); ++it) {
        if (*it == '\0') {
            p_string->erase(it, p_string->end());
            break;
        }
    }
    boost::algorithm::trim(*p_string);
}

// How to display an http_exception from cpprest:
static std::ostream& operator<<(std::ostream& stream,  // NOLINT
                                const web::http::http_exception& e) {
    stream << "http_exception: " << e.what();
    if (e.error_code().value() != 0) {
        // may have leading \r\0\0\0\0....\0
        std::string msg = e.error_code().message();
        TrimTrailingNulls(&msg);
        stream << " (" << e.error_code() << ": " << msg << ")";
    }
    return stream;
}

static std::string ExceptionToString(const web::http::http_exception& e) {
    std::ostringstream tmp;
    tmp << e;
    return tmp.str();
}

static std::ostream& operator<<(std::ostream& stream,  // NOLINT
                                const std::system_error& e) {
    std::string msg = e.code().message();
    TrimTrailingNulls(&msg);  // in case
    stream << "system_error: " << e.what()
           << " (" << e.code() << ": " << msg << ")";
    return stream;
}

static std::string ExceptionToString(const std::system_error& e) {
    std::ostringstream tmp;
    tmp << e;
    return tmp.str();
}

static std::ostream& operator<<(std::ostream& stream,
                                const std::exception& se) {
    stream << typeid(se).name() << ": " << se.what();
    return stream;
}

static std::string ExceptionToString(const std::exception& se) {
    std::ostringstream tmp;
    tmp << se;
    return tmp.str();
}


/**
 * \brief Add boost detailed exception info, if available.
 */
static void AddBoostDetailedInfo(std::ostream *p_out) {
    boost::exception const *p_be =
                       boost::current_exception_cast<boost::exception const>();
    if (!p_be) {
        return;
    }
    char const * const * f = boost::get_error_info<boost::throw_file>(*p_be);
    int const * l = boost::get_error_info<boost::throw_line>(*p_be);
    char const * const * fn =
                           boost::get_error_info<boost::throw_function>(*p_be);
    if (f && l && fn) {
        *p_out << std::endl << "  ";
        *p_out << *f;
        *p_out << '(' << *l << "): ";
        *p_out << "Throw in function ";
        *p_out << *fn;
    }
}

std::string CurrentExceptionToString() {
    return ExceptionPtrToString(std::current_exception());
}

std::string ExceptionPtrToString(const std::exception_ptr p_exception) {
    if (!p_exception) {
        return "[exception_ptr=nullptr ?!]";
    }
    try {
        std::rethrow_exception(p_exception);
    }
    catch (web::http::http_exception& e) {
        return ExceptionToString(e);
    }
    catch (std::system_error& e) {
        return ExceptionToString(e);
    }
    catch (CStorageException& cse) {
        std::ostringstream tmp;
        tmp << cse.ToString();
        AddBoostDetailedInfo(&tmp);

        // Add cause information, if exists:
        if (cse.cause()) {
            tmp << std::endl << "Caused by: ";
            tmp << ExceptionPtrToString(cse.cause());
        }
        return tmp.str();
    }
    catch (std::exception& se) {
        std::ostringstream tmp;
        tmp << ExceptionToString(se);
        AddBoostDetailedInfo(&tmp);
        return tmp.str();
    }
    catch (...) {
        return "Unknown exception object ?!";
    }
    return "should not happen";
}

}  // namespace pcs_api
