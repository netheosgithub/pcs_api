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

#ifndef INCLUDE_PCS_API_INTERNAL_C_RESPONSE_H_
#define INCLUDE_PCS_API_INTERNAL_C_RESPONSE_H_

#include <string>
#include <sstream>

#include "boost/property_tree/ptree.hpp"
#include "cpprest/json.h"
#include "cpprest/astreambuf.h"
#include "cpprest/http_msg.h"

#include "pcs_api/types.h"
#include "pcs_api/byte_sink.h"
#include "pcs_api/c_exceptions.h"

namespace pcs_api {

/**
 * \brief Class holding an http_response, with some methods to get body as json,
 *        xml or stream.
 *
 * This class holds an http response, and also the client used to perform the
 * request.
 * Client is released in destructor, by calling the provided function (either
 * a real destructor, or putting client back to a http clients pool).
 * Destructor ensures that http_response is fully read or canceled so that all
 * resources are freed.
 */
class CResponse {
 public:
     CResponse(web::http::client::http_client *p_client,
               std::function<void(web::http::client::http_client*)> release_c_f,
               const web::http::http_request& request,
               web::http::http_response *p_response,
               const pplx::cancellation_token_source& cancel_source);
    CResponse(const CResponse& other) = delete;
    CResponse& operator=(const CResponse& other) = delete;
    ~CResponse();

    int status() const {
        return status_;
    }

    const std::string& reason() const {
        return reason_;
    }

    const std::string& method() const {
        return method_;
    }

    const web::uri& uri() const {
        return uri_;
    }

    /**
     *
     * @return entity content length (0 if response has no body),
     *         or a negative number if unknown.
     */
    int64_t content_length() const {
        return content_length_;
    }

    /**
     * @return A reference to the response headers
     */
    const web::http::http_headers& headers() const {
        return response_.headers();
    }

    /**
     * \brief Throw a CStorageException, depending on http status code.
     *
     */
    void ThrowCStorageException(std::string message,
                                const CPath* p_opt_path) const;

    /**
    * \brief Get the response body as a binary string.
    *
    * This method can be called only once.
    *
    * @return The body value
    */
    const std::string AsString();

    /**
     * \brief Inquire if content type is json.
     *
     * If content type is json, the AsJson method can be called.
     *
     * @return true if content type is json like, false otherwise.
     */
    bool IsJsonContentType() const;

    /**
     * \brief Ensure content type is json.
     *
     * If no content-type is defined, or content-type is not json,
     * raises a CHttpException.
     *
     * @param throw_retriable if True, thrown exception is wrapped into
     *        a CRetriableException.
     */
    void EnsureContentTypeIsJson(bool throw_retriable) const;

    /**
     * \brief Get the response body as a json value.
     *
     * This method can be called only once, and only if this response has json
     * content type.
     *
     * @return The json value
     */
    web::json::value AsJson();

    /**
    * \brief Inquire if content type is xml.
    *
    * If content type is xml, the AsDom method can be called.
    *
    * @return true if content type is json like, false otherwise.
    */
    bool IsXmlContentType() const;

    /**
    * \brief Ensure content type is xml.
    *
    * If no content-type is defined, or content-type is not xml,
    * raises a CHttpException.
    *
    * @param throwRetriable if True, raised exception is wrapped
    *        into a CRetriableException.
    */
    void EnsureContentTypeIsXml(bool raiseRetriable) const;

    /**
     * \brief Get the response as a DOM (aka boost ptree).
     *
     * @return The DOM document, as a boost property tree.
     */
    const boost::property_tree::ptree AsDom();

    /**
     * \brief Blob download: read body and write data into given sink.
     *
     * @param p_bs destination
     */
    void DownloadDataToSink(ByteSink *p_bs);

    /**
     * \brief Returns a summary of response as a string
     */
    std::string ToString() const;

 private:
    int status_;
    std::string reason_;
    std::string method_;
    web::uri uri_;
    std::string content_type_;
    int64_t content_length_;
    web::http::client::http_client *p_client_;
    std::function<void(web::http::client::http_client *)>
                                                      release_client_function_;
    web::http::http_response response_;
    pplx::cancellation_token_source cancel_source_;
};


}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_C_RESPONSE_H_

