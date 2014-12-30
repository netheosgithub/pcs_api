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

#include "boost/lexical_cast.hpp"
#include "boost/property_tree/xml_parser.hpp"
#include "boost/algorithm/string.hpp"

#include "cpprest/asyncrt_utils.h"
#include "cpprest/interopstream.h"

#include "pcs_api/internal/c_response.h"
#include "pcs_api/internal/uri_utils.h"
#include "pcs_api/internal/logger.h"


namespace pcs_api {


/**
 * Parse content length. This is different of headers.content_length()
 * which returns 0 is no header is present.
 * 
 * @param headers
 * @return content-length header value, or -1 if no header is present
 */
static int64_t ExtractContentLength(const web::http::http_headers& headers) {
    auto it = headers.find(web::http::header_names::content_length);
    if (it != headers.end()) {
        string_t length_str = it->second;
        return boost::lexical_cast<int64_t, string_t>(length_str);
    }
    // No header:
    return -1;
}

CResponse::CResponse(
        web::http::client::http_client *p_client,
        std::function<void(web::http::client::http_client *)> release_client_f,
        const web::http::http_request& request,
        web::http::http_response *p_response,
        const pplx::cancellation_token_source& cancel_source)
    : p_client_(p_client),
      release_client_function_(release_client_f),
      method_(utility::conversions::to_utf8string(request.method())),
      uri_(request.request_uri()),
      status_(p_response->status_code()),
      reason_(utility::conversions::to_utf8string(p_response->reason_phrase())),
      content_length_(ExtractContentLength(p_response->headers())),
      // this is not a real copy, as http_response uses the pimpl pattern:
      response_(*p_response),
      cancel_source_(cancel_source) {
    // small hack: hubic returns a content type of:
    // "application/json; charset=utf8" but cpprestsdk checks for "utf-8" so,
    // in case, we replace:
    // This may have been improved in newer cpprestsdk versions ?
    string_t content_type = p_response->headers().content_type();
    if (!content_type.empty()) {
        boost::algorithm::replace_all(content_type, U("utf8"), U("utf-8"));
        p_response->headers().set_content_type(content_type);
    }
    content_type_ = utility::conversions::to_utf8string(content_type);
}

void CResponse::ThrowCStorageException(std::string message,
                                       const CPath* p_opt_path) const {
    if (message.empty()) {
        // No message has been given:
        // in this case we take the status line as message (better than empty)
        message = std::to_string(status_) + " "
                  + utility::conversions::to_utf8string(reason_);
    }

    switch (status_) {
        case 401:
            BOOST_THROW_EXCEPTION(CAuthenticationException(message,
                                                status_,
                                                reason_,
                                                method_,
                                                UriUtils::ShortenUri(uri_)));

        case 404:
            message = "No file found at URL " + UriUtils::ShortenUri(uri_)
                      + " (" + message + ")";
            BOOST_THROW_EXCEPTION(CFileNotFoundException(message, *p_opt_path));

        default:
            BOOST_THROW_EXCEPTION(CHttpException(message,
                                                 status_,
                                                 reason_,
                                                 method_,
                                                 UriUtils::ShortenUri(uri_)));
    }
}

const std::string CResponse::AsString() {
    auto task = response_.extract_vector();
    std::vector<unsigned char> content = task.get();
    return std::string(content.begin(), content.end());
}

bool CResponse::IsJsonContentType() const {
    // would do the job but does not work (linker):
    // web::http::details::is_content_type_json()
    return content_type_.find("text/javascript") != std::string::npos
        || content_type_.find("application/json") != std::string::npos;
}

void CResponse::EnsureContentTypeIsJson(bool throwRetriable) const {
    std::string msg;
    if (content_type_.empty()) {
        msg = std::string("Undefined Content-Type in server response");
    } else if (!IsJsonContentType()) {
        msg = std::string("Content-Type is not json: " + content_type_);
    }
    if (!msg.empty()) {
        if (!throwRetriable) {
            ThrowCStorageException(msg, nullptr);
        } else {
            try {
                ThrowCStorageException(msg, nullptr);
            }
            catch(...) {
                BOOST_THROW_EXCEPTION(
                                CRetriableException(std::current_exception()));
            }
        }
    }
    // we go here if content type is json
}

web::json::value CResponse::AsJson() {
    pplx::task<web::json::value> json_value = response_.extract_json();
    return json_value.get();
}

bool CResponse::IsXmlContentType() const {
    return content_type_.find("text/xml") != std::string::npos
        || content_type_.find("application/xml") != std::string::npos;
}

void CResponse::EnsureContentTypeIsXml(bool throwRetriable) const {
    std::string msg;
    if (content_type_.empty()) {
        msg = std::string("Undefined Content-Type in server response");
    } else if (!IsXmlContentType()) {
        msg = std::string("Content-Type is not xml: " + content_type_);
    }
    if (!msg.empty()) {
        if (!throwRetriable) {
            ThrowCStorageException(msg, nullptr);
        } else {
            try {
                ThrowCStorageException(msg, nullptr);
            }
            catch (...) {
                BOOST_THROW_EXCEPTION(
                                CRetriableException(std::current_exception()));
            }
        }
    }
    // we go here if content type is xml
}

const boost::property_tree::ptree CResponse::AsDom() {
    EnsureContentTypeIsXml(false);
    // We'll parse directly from the response stream
    // The other way would be to read body into string, and create stringstream
    // from it.
    // However internally, read_xml() reads the whole stream into a vector.
    // Note that this way does not permit to dump server raw response.
    // Either way, parsing XML with MSVC 2013 generates errors with DrMemory,
    // probably related to
    // https://connect.microsoft.com/VisualStudio/feedback/details/829931/vs2012-and-vs2013-istream-code-reads-off-the-end-of-its-non-null-terminated-stack-copied-stringParsing
    concurrency::streams::istream is = response_.body();
    // Need to adapt from asynchronous stream to std stream though:
    concurrency::streams::async_istream<char> std_is(is);
    boost::property_tree::ptree ret;
    boost::property_tree::read_xml(std_is, ret);
    return ret;
}

void CResponse::DownloadDataToSink(ByteSink *p_bs) {
    try {
        if (content_length_ >= 0) {
            // content length is known: inform any listener
            p_bs->SetExpectedLength(content_length_);
        }

        int64_t current = 0;
        concurrency::streams::istream is = response_.body();
        // The stream itself will not really be used, only its streambuf,
        // so we can't check for errors in this stream:
        std::ostream *p_os = p_bs->OpenStream();
        // Wrap output stream as asynchronous for cpprest API:
        concurrency::streams::stdio_ostream<char> os_wrapper(*p_os);
        concurrency::streams::streambuf<char> os_streambuf =
                                                        os_wrapper.streambuf();
        while (true) {
            // Read bytes directly into our output streambuf:
            size_t nb_read = is.read(os_streambuf, 2048).get();
            if (nb_read == 0) {
                break;  // end of body stream, or error
            }
            current += nb_read;
        }
        // FIXME error detection does not seem to be possible:
        // see https://casablanca.codeplex.com/discussions/561562
        // and https://casablanca.codeplex.com/workitem/244
        // For now, as all downloads should have a known content length,
        // we just compare this content length with the number of bytes
        // actually downloaded:
        if (content_length_ >= 0
            && current != content_length_) {
            // We have no details on error :(
            std::string msg =
                std::string("Did not write all bytes to sink (Content-Length=")
                + std::to_string(content_length_)
                + ", written=" + std::to_string(current) + ")";
            BOOST_THROW_EXCEPTION(CStorageException(msg));
        }
        os_wrapper.flush().get();  // void ?!
        p_os->flush();
        if (p_os->bad()) {
            BOOST_THROW_EXCEPTION(
                        CStorageException("Could not flush output stream"));
        }

        if (content_length_ < 0) {
            // content length was unknown;
            // inform listener operation is terminated
            // We should never pass here
            p_bs->SetExpectedLength(current);
        }
        p_bs->CloseStream();
    }
    catch (...) {
        LOG_ERROR << "Exception during download: "
                  << CurrentExceptionToString() << std::endl;
        p_bs->Abort();
        p_bs->CloseStream();
        throw;
    }
}

std::string CResponse::ToString() const {
    std::ostringstream tmp;
    tmp << method() << " "
        << utility::conversions::to_utf8string(uri().to_string())
        << " [" << status() << "/" << reason() << "] ("
        << content_type_ << ")";
    return tmp.str();
}

/**
 * A CResponse object may be destroyed _before_ response has been fully
 * consumed. In this case we cancel request.
 */
CResponse::~CResponse() {
    try {
        LOG_TRACE << "~CResponse : canceling token...";
        cancel_source_.cancel();  // no-op if body has already been read
    }
    catch(...) {
        LOG_WARN << "Could not cancel request: " << CurrentExceptionToString();
    }
    release_client_function_(p_client_);
}

}  // namespace pcs_api
