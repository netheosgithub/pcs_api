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
import logging
import threading

from ..storage import IStorageProvider, register_provider
from ..cexceptions import CRetriableError, CAuthenticationError
from ..oauth.oauth2_params import OAuth2ProviderParameters
from ..oauth.session_managers import OAuth2SessionManager
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..utils import (buildCStorageError, ensure_content_type_is_json)
from .swift import SwiftClient

logger = logging.getLogger(__name__)


@register_provider
class HubicStorage(IStorageProvider):

    PROVIDER_NAME = 'hubic'

    # hubiC endpoints:
    ROOT = 'https://api.hubic.com'
    ENDPOINT = ROOT + '/1.0'

    # OAuth2 endpoints and parameters:
    _oauth2ProviderParameters = OAuth2ProviderParameters(authorize_url=ROOT + '/oauth/auth/',
                                                         access_token_url=ROOT + '/oauth/token/',
                                                         refresh_token_url=ROOT + '/oauth/token/',
                                                         scope_in_authorization=True,
                                                         scope_perms_separator=',')

    def __init__(self, storage_builder):
        self._session_manager = OAuth2SessionManager(self._oauth2ProviderParameters,
                                                     storage_builder.app_info,
                                                     storage_builder.user_creds_repos,
                                                     storage_builder.user_credentials)
        self._retry_strategy = storage_builder.retry_strategy
        self._swift_client_lock = threading.Lock()
        self._swift_client = None

    def _buildCStorageError(self, response, c_path):
        # Try to extract error message from json body:
        # (this body can be json even if content type header is text/html !)
        # { "error":"invalid_token", "error_description":"not found" }
        server_error_msg = None
        try:
            json = response.json()
            server_error_msg = json['error']
            server_error_msg += ' (' + json['error_description'] + ')'
        except:
            pass
        return buildCStorageError(response, server_error_msg, c_path)

    def _validate_hubic_api_response(self, response, c_path):
        """A response is valid if server code is 2xx and content-type JSON.
        It is recoverable in case of server error 5xx."""
        logger.debug("validating hubic response: %s %s: %d %s",
                     response.request.method,
                     response.request.url,
                     response.status_code, response.reason)

        if response.status_code >= 500:
            raise CRetriableError(self._buildCStorageError(response, c_path))

        if response.status_code >= 300:
            raise self._buildCStorageError(response, c_path)

        # Server has answered 2xx, looks correct ; however check content type is json:
        # (sometimes hubic spuriously redirects to html page /error.xml)
        ensure_content_type_is_json(response, raise_retriable=True)
        # OK, response looks fine:
        return response

    def _get_request_invoker(self, validation_function, c_path):
        request_invoker = HubicRequestInvoker(self._session_manager, validation_function, c_path)
        return request_invoker

    def _get_api_request_invoker(self, c_path=None):
        return self._get_request_invoker(self._validate_hubic_api_response, c_path)

    def _get_swift_client(self):
        """Return current swift client, or create one if none exists yet.

        The internal swift client does NOT retry its requests.
        Retries are performed by this class."""

        sw_client = self._swift_client
        if sw_client:
            return sw_client

        # No client yet (or has been invalidated)
        with self._swift_client_lock:
            # Only a single thread should create this client
            sw_client = self._swift_client
            if sw_client:
                # Another thread created a new client: use it then
                logger.debug('Not created swift client in this thread, already done')
                return sw_client
            # This thread has to do it:
            ri = self._get_api_request_invoker()
            info = self._retry_strategy.invoke_retry(ri.get,
                                                     self.ENDPOINT + '/account/credentials').json()
            sw_client = SwiftClient(info['endpoint'],
                                    info['token'],
                                    retry_strategy=NoRetryStrategy(),
                                    with_directory_markers=True)
            sw_client.use_first_container()
            self._swift_client = sw_client
            return sw_client

    def _swift_caller(self, function, *args, **kwargs):
        """In case swift authentication token has expired, cleanup client
        before retrying, in order to get fresh credentials from hubiC API"""

        # This does its own retry logic (and may raise, in case of 401 error or so):
        sw_client = self._get_swift_client()

        try:
            return function(sw_client, *args, **kwargs)
        except CAuthenticationError as e:
            logger.warn("Swift authentication error: swift client invalidated")
            self._swift_client = None
            # Wrap as a retriable error without wait, so that retrier will not abort:
            raise CRetriableError(e, delay=0)

    def provider_name(self):
        return HubicStorage.PROVIDER_NAME

    def get_user_id(self):
        """user_id is email in case of hubic"""
        ri = self._get_api_request_invoker()
        json = self._retry_strategy.invoke_retry(ri.get, self.ENDPOINT + '/account').json()
        return json['email']

    def get_quota(self):
        """Return a CQuota object"""
        ri = self._get_api_request_invoker()
        json = self._retry_strategy.invoke_retry(ri.get, self.ENDPOINT + '/account/usage').json()
        return CQuota(json['used'], json['quota'])

    def list_root_folder(self):
        return self.list_folder(CPath('/'))

    def list_folder(self, c_folder_or_c_path):
        return self._retry_strategy.invoke_retry(self._swift_caller,
                                                 self._do_list_folder, c_folder_or_c_path)

    def _do_list_folder(self, sw_client, c_folder_or_c_path):
        return sw_client.list_folder(c_folder_or_c_path)

    def create_folder(self, c_path):
        return self._retry_strategy.invoke_retry(self._swift_caller,
                                                 self._do_create_folder, c_path)

    def _do_create_folder(self, sw_client, c_path):
        return sw_client.create_folder(c_path)

    def delete(self, c_path):
        return self._retry_strategy.invoke_retry(self._swift_caller,
                                                 self._do_delete, c_path)

    def _do_delete(self, sw_client, c_path):
        return sw_client.delete(c_path)

    def get_file(self, c_path):
        return self._retry_strategy.invoke_retry(self._swift_caller,
                                                 self._do_get_file, c_path)

    def _do_get_file(self, sw_client, c_path):
        return sw_client.get_file(c_path)

    def download(self, download_request):
        return self._retry_strategy.invoke_retry(self._swift_caller,
                                                 self._do_download, download_request)

    def _do_download(self, sw_client, download_request):
        return sw_client.download(download_request)

    def upload(self, upload_request):
        return self._retry_strategy.invoke_retry(self._swift_caller,
                                                 self._do_upload, upload_request)

    def _do_upload(self, sw_client, upload_request):
        return sw_client.upload(upload_request)


class HubicRequestInvoker(RequestInvoker):
    """This special class is for refreshing access_token once if we get
    an authentication error (it seems to happen that hubic gives sometimes
    invalid access_tokens ?!)."""
    def __init__(self, session_manager, validate_func, c_path):
        super(HubicRequestInvoker, self).__init__(c_path)
        self._session_manager = session_manager
        self._already_refreshed_token = False
        self._validate_func = validate_func

    def do_request(self, *args, **kwargs):
        return self._session_manager.do_request(*args, **kwargs)

    def validate_response(self, response, c_path):
        try:
            self._validate_func(response, c_path)
        except CAuthenticationError as ae:
            # Request has failed with an authentication problem:
            # as tokens expiration dates are checked before requests,
            # this should not occur (but in practice, it has been seen)
            logger.warn('Got an unexpected CAuthenticationError: %s', ae)
            if not self._already_refreshed_token:
                # If we didn't try already, get a new access_token:
                logger.warn('Will refresh access_token (in case it is broken?)')
                self._session_manager.refresh_token()
                self._already_refreshed_token = True
                # We force connection closing in that case,
                # to avoid being sticked on a non-healthy server:
                response.connection.close()  # this actually closes whole connections pool
                raise CRetriableError(ae, delay=0)
            raise ae
        return response


class NoRetryStrategy(RetryStrategy):
    """A strategy that never retries requests, and throws directly exceptions
    (without unwrapping CRetriableError).

    Used by underlying swift client so that we get back retriable exceptions here"""
    def __init__(self):
        super(NoRetryStrategy, self).__init__(1, 0)

    def invoke_retry(self, request_function, *args, **kwargs):
        return request_function(*args, **kwargs)
