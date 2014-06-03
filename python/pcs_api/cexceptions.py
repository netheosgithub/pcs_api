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


class CStorageError(Exception):
    """Base class for all cloud storage errors.
    Such exceptions have an optional 'message' and 'cause' attribute."""
    def __init__(self, message, cause=None):
        super(CStorageError, self).__init__(message)
        self.cause = cause

    def __str__(self):
        ret = "%s(%s)" % (self.__class__, self.message)
        if self.cause:
            ret += " (caused by %r)" % (self.cause,)
        return ret


class CInvalidFileTypeError(CStorageError):
    """Raised when performing an operation on a folder when a blob is expected,
    or when operating on a blob and a folder is expected.
    Also raised when downloading provider special files (eg google drive native docs)."""

    def __init__(self, c_path, expected_blob, message=None):
        """:param c_path: the problematic path
        :param expected_blob: if True, a blob was expected but a folder was found.
                              if False, a folder was expected but a blob was found
        :param message: optional message"""
        if not message:
            message = 'Invalid file type at %r (expected %s)' % \
                      (c_path, 'blob' if expected_blob else 'folder')
        super(CInvalidFileTypeError, self).__init__(message)
        self.path = c_path
        self.expected_blob = expected_blob


class CRetriableError(CStorageError):
    """Raised by RequestInvoker validation method, when request
    has failed but should be retried.

    This class is only a marker ; the underlying root exception
    is given by the 'cause' attribute.
    The optional 'delay' specifies how much one should wait before retrying"""
    def __init__(self, cause, delay=None):
        super(CRetriableError, self).__init__(message=None, cause=cause)
        self.delay = delay

    def get_delay(self):
        return self.delay

    #def __str__(self):
    #    return "%s(%s)" % (self.__class__, self.cause)


class CFileNotFoundError(CStorageError):
    """File has not been found (sometimes consecutive to http 404 error)"""
    def __init__(self, message, c_path):
        super(CFileNotFoundError, self).__init__(message)
        self.path = c_path


class CHttpError(CStorageError):
    """Raised when providers server answers non OK answers"""
    def __init__(self, request_method,
                 request_path,
                 status_code, reason,
                 message=None):
        super(CHttpError, self).__init__(message)
        self.request_method = request_method
        self.request_path = request_path
        self.status_code = status_code
        self.reason = reason

    def __str__(self):
        ret = "%s(%d %s) %s %s" % (
            self.__class__.__name__, self.status_code, self.reason, self.request_method, self.request_path )
        if self.message:
            ret += ' msg=%s' % self.message
        return ret

class CAuthenticationError(CHttpError):
    """http 401 error"""
    pass


