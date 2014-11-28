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

#ifndef INCLUDE_PCS_API_C_EXCEPTIONS_H_
#define INCLUDE_PCS_API_C_EXCEPTIONS_H_

#include <string>
#include <chrono>

#include "boost/exception/exception.hpp"

#include "pcs_api/c_path.h"


namespace pcs_api {

/**
 * \brief Generic pcs_api storage exception.
 */
struct CStorageException: virtual std::exception, virtual boost::exception {
 public:
    CStorageException();
    explicit CStorageException(const std::string& message);
    CStorageException(const std::string& message, std::exception_ptr cause);
    virtual const char *what() const _noexcept override {
        return message_.c_str();
    }
    std::exception_ptr cause() const {
        return p_cause_;
    }
    /**
     * @return A nice message for this exception
     *         (mainly for developers or support)
     */
    virtual std::string ToString() const;
 private:
    const std::string message_;
    const std::exception_ptr p_cause_;
};

/**
 * \brief Thrown when provider has answered a valid but unexpected
 *        http response.
 *
 * "unexpected" means a bad status code, or invalid content-type, etc. (reasons
 * are provider dependent).
 */
struct CHttpException: CStorageException {
 public:
    CHttpException(const std::string& message,
                   int status,
                   const std::string& reason,
                   const std::string& method,
                   const std::string& url);
    virtual std::string ToString() const override;
    int status() const {
        return status_;
    }
    std::string reason() const {
        return reason_;
    }
    std::string request_method() const {
        return request_method_;
    }
    std::string url() const {
        return url_;
    }

 private:
    int status_;
    std::string reason_;
    std::string request_method_;
    std::string url_;
};

/**
 * \brief Thrown when provider has answered a 401 status code.
 * 
 * Only thrown if credentials are invalid, or provider is currently in trouble.
 */
struct CAuthenticationException: CHttpException {
 public:
    CAuthenticationException(const std::string& message,
                             int status,
                             const std::string& reason,
                             const std::string& method,
                             const std::string& url);
    std::string ToString() const override;
};

/**
 * \brief Thrown when an operation is attempted to a non existing file.
 *
 * Not all missing files errors result in this exception being thrown: for now,
 * only if trying to download a remote file that does not exist.
 */
struct CFileNotFoundException: CStorageException {
 public:
    CFileNotFoundException(const std::string& message, const CPath& path);
    std::string ToString() const override;
    CPath path() const {
        return path_;
    }
 private:
    const CPath path_;
};

/**
 * \brief Thrown when a path references a folder but a blob is expected
 * (ex: upload or download operations), or vice-versa (ex: listing a folder).
 * Also thrown for provider specific files that can not be downloaded
 * (ex: google documents).
 *
 * Exception contains getters to the faulty path and a boolean indicating if
 * operation expects a blob (true) or a folder (false).
 *
 * @param path
 * @param blob_expected
 */
struct CInvalidFileTypeException: CStorageException {
 public:
    CInvalidFileTypeException(const CPath& path, bool blob_expected);
    std::string ToString() const override;
    CPath path() const {
        return path_;
    }
    bool blob_expected() const {
        return blob_expected_;
    }
 private:
    CPath path_;
    bool blob_expected_;
};

/**
 * \brief Internal non fatal exception marker.
 * 
 * This is only an internal marker/wrapper class to indicate that cause
 * exception is not fatal and may be retried.
 */
struct CRetriableException: CStorageException {
 public:
    explicit CRetriableException(std::exception_ptr p_cause,
             std::chrono::milliseconds delay = std::chrono::milliseconds(-1));
    std::string ToString() const override;
    /**
     * \brief Get optional delay before try.
     * 
     * Some non fatal errors (token expiration) can be retried immediately,
     * instead of waiting with standard exponential backoff.
     * 
     * @return number of milliseconds to wait before retrying,
     *         or -1 if undefined.
     */
    std::chrono::milliseconds delay() const {
        return delay_;
    }

 private:
    const std::chrono::milliseconds delay_;
};

/**
 * \brief Get current exception human description.
 * 
 * @return a (hopefully) nice string about current exception
 *         (to be called in a catch block only)
 */
std::string CurrentExceptionToString();

/**
 * \brief Get exception human description.
 *
 * @return a (hopefully) nice string about given exception.
 */
std::string ExceptionPtrToString(const std::exception_ptr p_exception);

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_C_EXCEPTIONS_H_

