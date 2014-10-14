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
import re
import urllib
import random
import time
import logging
import requests

from .bytes_io import ProgressByteSource, ProgressByteSink
from .cexceptions import CRetriableError


class CPath(object):
    """Immutable remote file pathname, with methods for easier handling.

    path components are separated by a single slash. A path is always unicode
    and normalized so that it begins by a leading slash and never ends with
    a trailing slash, except for root path: '/'.
    Some characters are forbidden in CPath objects: backslash and all chars
    with code point < 32.
    Spaces are forbidden at start and end of each pathname component."""

    FORBIDDEN_CHARS = """\\"""

    def __init__(self, pathname):
        if isinstance(pathname, str):
            pathname = unicode(pathname, 'UTF-8')
        self._check(pathname)
        self._pathname = self._normalize(pathname)

    def path_name(self):
        """Return this full path as string"""
        return self._pathname

    def url_encoded(self):
        """Return this full path string encoded for URL path (slashes are not encoded)"""
        return urllib.quote(self._pathname.encode('UTF-8'))

    def base_name(self):
        """Return last element of this path as string (empty string if this object is root path)"""
        l = self._pathname.rfind('/')
        return self._pathname[l+1:]

    def is_root(self):
        return self._pathname == '/'

    def parent(self):
        """CPath of this parent folder, or root folder if this path is root folder"""
        l = self._pathname.rfind('/')
        return CPath(self._pathname[0:l])

    def add(self, basename):
        # Add a path component:
        return CPath(self._pathname + '/' + basename)

    def split(self):
        """Returns a list of path segments (empty list if root folder)"""
        if self.is_root():
            return []
        return self._pathname[1:].split('/')

    def __eq__(self, obj):
        return isinstance(obj, CPath) and obj._pathname == self._pathname

    def __ne__(self, obj):
        return not self == obj

    def __hash__(self):
        return hash(self._pathname)

    @staticmethod
    def _check(pathname):
        """Check pathname is valid"""
        for c in pathname:
            if ord(c) < 32 or c in CPath.FORBIDDEN_CHARS:
                raise ValueError("pathname contains invalid char %r: %r" % (c, pathname))
        # Check each component:
        for comp in pathname.split('/'):
            if comp.strip() != comp:
                raise ValueError("pathname contains leading or trailing spaces: %r" % pathname)

    @staticmethod
    def _normalize(pathname):
        """Replace double slashes with single slash,
        remove any trailing slash except for root path '/'."""
        pathname = re.sub('/+', '/', pathname)
        pathname = '/' + pathname.strip('/')
        return pathname

    def __repr__(self):
        return 'CPath(%r)' % self._pathname


class CFile(object):

    def __init__(self, path, file_id=None, modification_time=None, metadata=None):
        self.path = path
        self.file_id = file_id
        self.modification_time = modification_time
        self.metadata = metadata

    def is_folder(self):
        raise NotImplementedError

    def is_blob(self):
        raise NotImplementedError


class CFolder(CFile):

    def is_folder(self):
        return True

    def is_blob(self):
        return False

    def __repr__(self):
        return 'CFolder(%s)' % repr(self.path)


class CBlob(CFile):

    def __init__(self, length, content_type, *args, **kwargs):
        super(CBlob, self).__init__(*args, **kwargs)
        self.length = length
        self.content_type = content_type

    def is_folder(self):
        return False

    def is_blob(self):
        return True

    def __repr__(self):
        return 'CBlob(%s) %s (%d bytes)' % (repr(self.path), self.content_type, self.length)


class CUploadRequest(object):

    def __init__(self, path, byte_source):
        self.path = path
        self._byte_source = byte_source
        self._content_type = None
        self._metadata = None
        self._progress_listener = None

    def content_type(self, content_type):
        self._content_type = content_type
        return self

    def metadata(self, metadata):
        # TODO cloner dico ? pas nécessaire car cet objet sera détruit
        self._metadata = metadata
        return self

    def progress_listener(self, pl):
        self._progress_listener = pl
        return self

    def byte_source(self):
        """If no progress listener has been specified, returns the ByteSource set in constructor.
        Otherwise, decorate this ByteSource"""
        bs = self._byte_source
        if self._progress_listener:
            bs = ProgressByteSource(bs, self._progress_listener)
        return bs


class CDownloadRequest(object):

    def __init__(self, path, byte_sink):
        self.path = path
        self._byte_sink = byte_sink
        self._byte_range = None
        self._progress_listener = None

    def range(self, offset=None, length=None):
        """Define a range for partial content download. Note that second parameter is a length,
        not an offset (this differs from http header Range header raw value)
        If both parameters are None, full download is requested.
        :param offset: index of range start. If None, download last 'length' bytes
        :param length: length of range to download ; if None, download from offset up to end of resource.
        """
        if offset is not None or length is not None:
            self._byte_range = (offset, length)
        else:
            self._byte_range = None
        return self

    def progress_listener(self, pl):
        self._progress_listener = pl
        return self

    def get_http_headers(self):
        headers = {}
        if self._byte_range is not None:
            self._add_byte_range_header(headers)
        return headers

    def byte_sink(self):
        """If no progress listener has been specified, returns the ByteSink set in constructor.
        Otherwise, decorate this ByteSink"""
        bs = self._byte_sink
        if self._progress_listener:
            bs = ProgressByteSink(bs, self._progress_listener)
        return bs

    def _add_byte_range_header(self, headers):
        range_value = 'bytes='
        if self._byte_range[0] is not None:
            range_value += str(self._byte_range[0])
            start = self._byte_range[0]
        else:
            start = 1
        range_value += '-'
        if self._byte_range[1] is not None:
            range_value += str(start + self._byte_range[1] - 1)
        headers['Range'] = range_value


class RequestInvoker(object):
    """Object responsible of performing http request
    and validating http server response.

    Http requests are actually delegated to python 'requests' package."""

    def __init__(self, c_path=None):
        self.c_path = c_path

    def do_request(self, *args, **kwargs):
        """Called by invoke(): in charge of performing actual request
        and returning server response."""
        raise NotImplementedError

    def validate_response(self, response, c_path):
        """Called when do_invoke() returned a server response ; this method
        should validate and return response, or raise an Exception
        or a CRetriableError in case of error.

        Validation is provider-dependent (and sometimes method dependent)
        so this method is abstract."""
        raise NotImplementedError

    def invoke(self, *args, **kwargs):
        """The main method that performs http request.
        Two steps: requesting server with do_request(),
        then validating response with validate_response()."""
        try:
            response = self.do_request(*args, **kwargs)
        except Exception as e:
            if not self.on_request_error(e):
                raise
            raise CRetriableError(e)
        return self.validate_response(response, self.c_path)

    def on_request_error(self, e):
        """do_request() failed with a low level exception (socket, protocol error, timeout...).
        Determine if the request can be retried, or if we should give up right now.

        :returns True is request is retriable, False if not"""
        logging.debug('Request failed: %s' % e)
        # some requests low-level errors that may need to be retried:
        return (isinstance(e, requests.Timeout)
                or isinstance(e, requests.exceptions.ConnectionError)
                or isinstance(e, requests.exceptions.ChunkedEncodingError)
               )

    # all python requests.get(), put(), post()... methods are delegated to request(),
    # so we only hook this latter:
    def get(self, *args, **kwargs):
        """Simple forward to invoke() with method as first argument"""
        return self.invoke("GET", *args, **kwargs)

    def head(self, *args, **kwargs):
        """Simple forward to invoke() with method as first argument"""
        return self.invoke("HEAD", *args, **kwargs)

    def put(self, *args, **kwargs):
        """Simple forward to invoke() with method as first argument"""
        return self.invoke("PUT", *args, **kwargs)

    def post(self, *args, **kwargs):
        """Simple forward to invoke() with method as first argument"""
        return self.invoke("POST", *args, **kwargs)

    def delete(self, *args, **kwargs):
        """Simple forward to invoke() with method as first argument"""
        return self.invoke("DELETE", *args, **kwargs)


class RetryStrategy(object):
    """Object responsible of retrying request if it fails temporarily

    This object is immutable and shared between requests"""

    def __init__(self, nb_tries, first_sleep):
        self.nb_tries_max = nb_tries
        self.first_sleep = first_sleep
        # for unit tests:
        self._rand = random.random
        self._sleep = time.sleep

    def invoke_retry(self, request_function, *args, **kwargs):
        """Main method to be called by user of this class:
        calls request_invoker.invoke(*args, **kwargs) until success,
        non retriable error, or max trials has been reached."""
        current_tries = 0
        while True:
            current_tries += 1
            response = None
            try:
                return request_function(*args, **kwargs)
            except CRetriableError as e:
                if current_tries >= self.nb_tries_max:
                    logging.warning("Aborting %s after %d failed attempts: %s",
                                    request_function, current_tries, e)
                    raise e.cause
                logging.warning('Will retry after failed request: %s', e)
                self.wait(current_tries, e.get_delay())

    def wait(self, current_tries, duration=None):
        """Sleep before retry ; default implementation is exponential back-off,
        or the specified duration"""
        if duration is None:
            duration = self.first_sleep * (self._rand()+0.5) * 2 ** (current_tries - 1)
        logging.debug('Will retry request after %.1f s' % duration)
        self._sleep(duration)


class CQuota(object):
    """A simple class containing used/available storage informations.

    Negative values indicate that the information is not available from provider."""

    def __init__(self, used_bytes, allowed_bytes):
        self.used_bytes = used_bytes
        self.allowed_bytes = allowed_bytes

    def percent_used(self):
        """Return used space as a float percentage (or -1.0 if unknown values)"""
        if self.used_bytes >= 0 and self.allowed_bytes > 0:
            return self.used_bytes*100.0 / self.allowed_bytes
        else:
            return -1.0

    def __repr__(self):
        return 'CQuota(%d, %d)' % (self.used_bytes, self.allowed_bytes)
