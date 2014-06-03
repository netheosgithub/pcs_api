# -*- coding: utf-8 -*-
#
# Copyright (c) 2014 Netheos (http://www.netheos.net)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


from __future__ import absolute_import, unicode_literals, print_function
import urlparse
# Part of this file forked from requests_toolbelt.multipart,
# Copyright 2014 Ian Cordasco, Cory Benfield, licensed under Apache License 2.0.
from requests.utils import super_len
from requests.packages.urllib3.filepost import iter_field_objects
from requests.packages.urllib3.fields import RequestField
from uuid import uuid4

import io

from .cexceptions import (CRetriableError, CHttpError,
                          CInvalidFileTypeError, CFileNotFoundError, CAuthenticationError)


def abbreviate(a_string, max_len=100):
    """Return a truncated string if its length exceeds max_len.
    Ellipsis dots are added if truncated (and total length is max_len + 3)."""
    return a_string[:max_len] + (a_string[max_len:] and '...')

def _ensure_content_type_in(content_types, requests_response, raise_retriable=True ):
    """Extract content type from response headers, and ensure
    it is acceptable, ie. at least one element in content_types list is contained in content-type.
    If no content-type is defined, or content-type is not acceptable, raises a CHttpError.
    :param content_types: list of acceptable content-type (substring)
    :param requests_response: the response to check
    :param raise_retriable: if True, raised exception is wrapped into a CRetriable"""
    try:
        actual_ct = requests_response.headers['Content-Type']
    except KeyError:
        e = buildCStorageError(requests_response, None, "Undefined Content-Type in server response")
        if raise_retriable:
            e = CRetriableError(e)
        raise e
    if not any(ct in actual_ct for ct in content_types):
        e = buildCStorageError(requests_response, None, "Content-Type is not json: %s" % actual_ct)
        if raise_retriable:
            e = CRetriableError(e)
        raise e


def ensure_content_type_is_json(requests_response, raise_retriable=True):
    _ensure_content_type_in(['application/json', 'text/javascript'], requests_response, raise_retriable)


def ensure_content_type_is_xml(requests_response, raise_retriable):
    _ensure_content_type_in(['application/xml', 'text/xml'], requests_response, raise_retriable)


def get_content_length(requests_response):
    """Extract content length from response headers. Returns None if no Content-Length header"""
    try:
        length = requests_response.headers['Content-Length']
        return int(length)
    except KeyError:
        return None


def get_content_type(requests_response):
    """Extract content type from response headers. Returns None if no content-type"""
    try:
        ct = requests_response.headers['Content-Type']
        return ct
    except KeyError:
        return None


def shorten_url(url):
    """Remove query parameters from url
    (only for logging, as query parameters may contain sensible informations)"""
    elements = urlparse.urlsplit(url)
    short_url = "%s://%s%s" % (elements[0], elements[1], elements[2])
    return short_url


def buildCStorageError(requests_response, message, c_path):
    """Some common code between providers.
    Handle the different status codes, and generate a nice exception"""

    # remove query string that may contain sensible informations:
    short_url = shorten_url(requests_response.request.url)

    # Some special cases here:
    if requests_response.status_code == 401:
        return CAuthenticationError(requests_response.request.method,
                                    short_url,
                                    requests_response.status_code,
                                    requests_response.reason,
                                    message)
    elif requests_response.status_code == 404:
        return CFileNotFoundError(message, c_path)
    else:
        return CHttpError(requests_response.request.method,
                          short_url,
                          requests_response.status_code,
                          requests_response.reason,
                          message)


def download_data_to_sink(response, byte_sink):
    """Server has answered OK with a file to download as stream.
    Open byte sink stream, and copy data from server stream to sink stream"""
    if 'Content-Length' in response.headers:
        total = int(response.headers['Content-Length'])
        byte_sink.set_expected_length(total)
    else:
        # chunked encoding: total bytes to transfer is not available...
        total = None
    current = 0
    out_stream = byte_sink.open_stream()
    success = False
    try:
        for chunk in response.iter_content(chunk_size=8*1024):
            current += len(chunk)
            # TODO write() contract says it may not write all bytes.
            # in practice it works with files or memory streams
            out_stream.write(chunk)
            # We are finished.
        # In case total was unknown beforehand, inform that everything is finished:
        if total is None:
            byte_sink.set_expected_length(current)
        out_stream.flush()
        success = True
    finally:
        if not success:
            out_stream.abort()
        out_stream.close()


class MultipartEncoder(object):

    """

    The ``MultipartEncoder`` oject is a generic interface to the engine that
    will create a ``multipart/form-data`` body for you.

    The basic usage is::

        import requests
        from requests_toolbelt import MultipartEncoder

        encoder = MultipartEncoder({'field': 'value',
                                    'other_field', 'other_value'})
        r = requests.post('https://httpbin.org/post', data=encoder,
                          headers={'Content-Type': encoder.content_type})

    If you do not need to take advantage of streaming the post body, you can
    also do::

        r = requests.post('https://httpbin.org/post',
                          data=encoder.to_string(),
                          headers={'Content-Type': encoder.content_type})

    """

    def __init__(self, fields, multipart_type='form-data', boundary=None, encoding='utf-8'):
        #: Type of multipart: form-data, related, etc.
        self.multipart_type = multipart_type

        #: Boundary value either passed in by the user or created
        self.boundary_value = boundary or uuid4().hex
        self.boundary = '--{0}'.format(self.boundary_value)

        #: Default encoding
        self.encoding = encoding

        #: Fields passed in by the user
        self.fields = fields

        #: State of streaming
        self.finished = False

        # Index of current field during iteration
        self._current_field_number = None

        # Most recently used data
        self._current_data = None

        # Length of the body
        self._len = None

        # Our buffer
        self._buffer = CustomBytesIO(encoding=encoding)

        # This a list of two-tuples containing the rendered headers and the
        # data.
        self._fields_list = []

        # Iterator over the fields so we don't lose track of where we are
        self._fields_iter = None

        # This field is set to True after the first read() has returned an empty array.
        # Used to detect a reuse of this object (in case of 401 request replay)
        self._finished = False

        # Pre-render the headers so we can calculate the length
        self._render_headers()

    def __len__(self):
        if self._len is None:
            self._calculate_length()

        return self._len

    def _calculate_length(self):
        boundary_len = len(self.boundary)  # Length of --{boundary}
        self._len = 0
        for (header, data) in self._fields_list:
            # boundary length + header length + body length + len('\r\n') * 2
            # note that in case header contains non-ascii chars, count must be based on encoded value:
            self._len += boundary_len + len(encode_with(header, self.encoding)) + super_len(data) + 4
        # Length of trailing boundary '--{boundary}--\r\n'
        self._len += boundary_len + 4

    @property
    def content_type(self):
        return str('multipart/{0}; boundary={1}'.format(
            self.multipart_type,
            self.boundary_value
            ))

    def to_string(self):
        return self.read()

    def read(self, size=None):
        """Read data from the streaming encoder.

        :param int size: (optional), If provided, ``read`` will return exactly
            that many bytes. If it is not provided, it will return the
            remaining bytes.
        :returns: bytes
        """
        if self._finished:
            # This object does not permit repetition. We detect this when
            # our client should have detected end of content, and call read() again.
            # We wrap in CRetriable so that request is fully rebuilt and restarted:
            raise CRetriableError(ValueError("MultipartEncoder content is not repeatable"))

        if size is not None and size < 0:
            size = None
        if size is not None:
            size = int(size)  # Ensure it is always an integer
            # We'll ask to read less from fields if our temp _buffer already has data:
            bytes_length = len(self._buffer)  # how many bytes remain in our temp buffer

            size -= bytes_length if size > bytes_length else 0

        self._load_bytes(size)
        ret = self._buffer.read(size)
        if not ret:
            self._finished = True
        return ret

    def _load_bytes(self, size):
        """Transfer bytes from source to temp _buffer, iterating over sources
        and adding parts separators when required (depending on size to read).
        Reset temp _buffer for read operation."""
        written = 0
        orig_position = self._buffer.tell()
        # we'll write at the end of _buffer (may not be empty):
        self._buffer.seek(0, io.SEEK_END)

        # Are we starting ?
        if self._current_field_number is None:
            written += self._buffer.write(
                encode_with(self.boundary, self.encoding)
            )
            written += self._buffer.write(encode_with('\r\n', self.encoding))
            self._current_field_number = 0

        # Consume data from current field, if any:
        written += self._consume_current_data(size)

        while size is None or written < size:
            # We must read all, or we have not read enough above, so read next part:
            next_tuple = self._next_tuple()
            #print "Next tuple: ", next_tuple
            if not next_tuple:
                self.finished = True
                break

            headers, data = next_tuple

            # We have a tuple, write the headers in their entirety.
            # They aren't that large, if we write more than was requested, it
            # should not hurt anyone much.
            written += self._buffer.write(encode_with(headers, self.encoding))
            self._current_data = coerce_data(data, self.encoding)
            if size is not None and written < size:
                size -= written
            written += self._consume_current_data(size)

        # Prepare next read operation in _buffer:
        self._buffer.seek(orig_position, 0)
        self._buffer.smart_truncate()

    def _consume_current_data(self, size):
        """Consume bytes from current field (_current_data, if any) to _buffer.
        If field is finished after operation, add parts separator.
        :returns number of bytes written to _buffer"""
        written = 0

        # File objects need an integer size
        if size is None:
            size = -1

        if self._current_data is not None:
            # and super_len(self._current_data) >= 0):
            # Read from field, copy to _buffer:
            written = self._buffer.write(self._current_data.read(size))
            #print "   written=",written,"  super_len(self._current_data) == ",super_len(self._current_data)
            if super_len(self._current_data) == 0:
                # we'we just finished current field: emit boundary now
                # and reset it to None to forget
                self._current_data = None
                written += self._buffer.write(
                    encode_with('\r\n{0}'.format(self.boundary),
                                self.encoding)
                    )
                # If this is the last separator add -- before \r\n:
                #print "END of field: current_number=",self._current_field_number
                if self._current_field_number == len(self.fields):
                    self._buffer.write(encode_with('--', self.encoding))
                self._buffer.write(encode_with('\r\n', self.encoding))

        return written

    def _next_tuple(self):
        next_tuple = tuple()

        try:
            # Try to get another field tuple
            next_tuple = next(self._fields_iter)
            self._current_field_number += 1
        except StopIteration:
            pass

        return next_tuple

    def _render_headers(self):
        e = self.encoding
        iter_fields = iter_field_objects(self.fields)
        self._fields_list = [
            (f.render_headers(), readable_data(f.data, e)) for f in iter_fields
            ]
        self._fields_iter = iter(self._fields_list)


def encode_with(string, encoding):
    """Encoding ``string`` with ``encoding`` if necessary.

    :param str string: If string is a bytes object, it will not encode it.
        Otherwise, this function will encode it with the provided encoding.
    :param str encoding: The encoding with which to encode string.
    :returns: encoded bytes object
    """
    if string and not isinstance(string, bytes):
        return string.encode(encoding)
    return string


def readable_data(data, encoding):
    if isinstance(data, RequestField):
        data = data.data

    if hasattr(data, 'read'):
        return data

    return CustomBytesIO(data, encoding)


def coerce_data(data, encoding):
    if not isinstance(data, CustomBytesIO):
        if hasattr(data, 'getvalue'):
            return CustomBytesIO(data.getvalue(), encoding)

        if hasattr(data, 'len'):  # All ByteSink InputStreams have this attribute
            return InputStreamWrapper(data, data.len)

    return data


class CustomBytesIO(io.BytesIO):
    def __init__(self, buffer=None, encoding='utf-8'):
        buffer = encode_with(buffer, encoding)
        super(CustomBytesIO, self).__init__(buffer)

    def _get_end(self):
        current_pos = self.tell()
        self.seek(0, 2)
        length = self.tell()
        self.seek(current_pos, 0)
        return length

    def __len__(self):
        length = self._get_end()
        return length - self.tell()

    def smart_truncate(self):
        to_be_read = len(self)
        already_read = self._get_end() - to_be_read

        if already_read >= to_be_read:
            old_bytes = self.read()
            self.seek(0, 0)
            self.truncate()
            self.write(old_bytes)
            self.seek(0, 0)  # We want to be at the beginning


class InputStreamWrapper(object):
    """Wraps an input stream of known length ;
    the len() python function will return the number of remaining bytes in stream"""
    def __init__(self, input_stream, length):
        self._stream = input_stream
        self._length = length

    def __len__(self):
        return self._length

    def read(self, length=-1):
        ret = self._stream.read(length)
        if ret is not None:
            self._length -= len(ret)
        return ret
