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

#ifndef INCLUDE_PCS_API_INTERNAL_MULTIPART_STREAMER_H_
#define INCLUDE_PCS_API_INTERNAL_MULTIPART_STREAMER_H_

#include <string>
#include <vector>

#include "pcs_api/types.h"
#include "pcs_api/byte_source.h"

namespace pcs_api {

/**
 * \brief An object that read underlying byte sources to dynamically generate
 * a http request form/multipart body.
 *
 * A MultiPartStreamer holds several "Part", each Part has some headers
 * and a ByteSource for content.
 * The streamer can be Reset(), so that body can be read several times.
 * This is used for example after a POST with a 401 response: cpprestsdk
 * re-issues request in this case, but first seeks stream back to 0.
 *
 * Parts streams are opened lazily (when the part actually needs to be read).
 * The side effect is that if the multipart stream is to be read again
 * (method Reset()), the parts streams will be closed and re-open
 * so are always seeked to the beginning position (most likely 0).
 */
class MultipartStreamer {
 public:
    /**
     * \brief Client object for defining a Part to be added to streamer.
     */
    class Part {
     public:
        Part(const std::string& name, ByteSource *p_source);
        void AddHeader(const std::string& name, const std::string& raw_value);
        const std::string& headers() const {
            return headers_;
        }
        ByteSource *source() const {
            return p_source_;
        }
        /**
         * @return length of this part (boundaries are not counted,
         *         only headers and content)
         */
        int64_t Length() const;
     private:
        const std::string name_;
        ByteSource * const p_source_;
        std::string headers_;  // already rendered
    };
    /**
     * Construct a multipart streamer object,
     * with content type set to "multipart/{type}" and a random boundary.
     */
    explicit MultipartStreamer(const std::string& type = "form-data");
    /**
     * Construct a multipart streamer object,
     * with content type set to "multipart/{type}"
     * and the given boundary (mainly useful for unit tests).
     */
    MultipartStreamer(const std::string& type, const std::string& boundary);
    /**
     * \brief Add a part (argument is copied).
     */
    void AddPart(const Part& part);
    /**
     * \brief Get content type for request.
     */
    string_t ContentType() const;
    /**
     * \brief Calculate content length for request.
     */
    int64_t ContentLength() const;
    /**
     * \brief Reset state so that we are able to read again the multipart body.
     */
    void Reset();
    /**
     * \brief Read some bytes of body.
     * @param p_data destination buffer
     * @param size destination buffer size
     * @return number of bytes written to p_data, 0 at EOF,
     *         or &lt; 0 in case of error.
     */
    std::streamsize ReadData(char *p_data, std::streamsize size);

 private:
    const std::string content_type_;  // form-data, related, alternative...
    // It helps to almost duplicate boundary here,
    // avoids tiny states for writing \r\n or leading --:
    const std::string first_boundary_;  // no leading \r\n, trailing \r\n
    const std::string not_first_boundary_;  // with leading and trailing \r\n
    const std::string final_boundary_;  // not_first_boundary_ with leading '--'
    /**
     * The parts that will be streamed.
     */
    std::vector<Part> parts_;
    /**
     * Current part, or parts_.end() when reading final boundary
     */
    std::vector<Part>::const_iterator parts_iterator_;
    /**
     * Current source stream, or empty if before or after sources
     */
    std::unique_ptr<std::istream> p_source_stream_;
    /**
     * Has the Read() operations begun ? Reset to false when Reset() is called.
     */
    bool started_;
    /**
     * when reading parts, where are we in current part ?
     * For first part, sequence of states is:
     *      kInPartFirstBoundary --> kInPartHeaders --> kInPartContent
     * For other parts, sequence of states is:
     *      kInPartNotFirstBoundary --> kInPartHeaders --> kInPartContent
     */
    enum PartReadingState {
        kInPartFirstBoundary,
        kInPartNotFirstBoundary,
        kInPartHeaders,
        kInPartContent
    } part_state_;
    /**
     * \brief offset in data currently written
     *        (either boundary, headers, data, or final boundary).
     */
    int64_t offset_;
    /**
     * \brief length of data currently written
     *        (either boundary, headers, data, or final boundary).
     */
    int64_t length_;

    /**
     * 
     * @param p_data dest buffer
     * @param size max nb of bytes to read
     * @return number of bytes read (may be 0), or &lt; 0 in case of error.
     */
    std::streamsize ReadDataFromPart(char *p_data, std::streamsize size);
    /**
     * 
     * @param p_data dest buffer
     * @param size max nb of bytes to read
     * @return number of bytes read, 0 at EOF, or &lt; 0 in case of error.
     */
    std::streamsize ReadDataFromFinalBoundary(char *p_data,
                                              std::streamsize size);
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_MULTIPART_STREAMER_H_

