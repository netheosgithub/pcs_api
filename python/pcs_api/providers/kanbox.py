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
import requests_oauthlib.compliance_fixes

from ..storage import IStorageProvider, register_provider
from ..oauth.oauth2_params import OAuth2ProviderParameters
from ..oauth.session_managers import OAuth2SessionManager
from ..utils import buildCStorageError, abbreviate
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import CRetriableError

logger = logging.getLogger(__name__)


@register_provider
class KanboxStorage(IStorageProvider):
    """FIXME : WORK IN PROGRESS"""

    PROVIDER_NAME = 'kanbox'

    # Kanbox endpoints:
    _OAUTH_ENDPOINT = 'https://auth.kanbox.com/0'
    ENDPOINT = 'https://api.kanbox.com/0'
    UPLOAD_ENDPOINT = 'https://api-upload.kanbox.com/0/upload'

    # OAuth2 endpoints and parameters:
    # There is no scope in kanbox: always full access ?
    _oauth2ProviderParameters = OAuth2ProviderParameters(authorize_url=_OAUTH_ENDPOINT + '/auth',
                                                         access_token_url=_OAUTH_ENDPOINT + '/token',
                                                         refresh_token_url=_OAUTH_ENDPOINT + '/token',
                                                         scope_in_authorization=False)

    def __init__(self, storage_builder):
        self._session_manager = OAuth2SessionManager(self._oauth2ProviderParameters,
                                                     storage_builder.app_info,
                                                     storage_builder.user_creds_repos,
                                                     storage_builder.user_credentials)
        # kanbox is not compliant, we must add a session hook:
        self._session_manager.get_session = _decorate(self._session_manager.get_session)
        self._retry_strategy = storage_builder.retry_strategy

    def _buildCHttpError(self, response, json=None, message=None, c_path=None):
        # Try to extract error message from json body : looks like
        # {"status":"error","errorCode":"ERROR_PATH_NOT_EXIST"}
        server_error_msg = response.text  # may be empty
        try:
            if response.status_code == 401:
                # Error description is in WWW-Authenticate: header, looks like :
                # WWW-Authenticate: OAuth realm='Service', error='expired_token', error_description='The access token provided has expired'
                try:
                    server_error_msg = response.headers['WWW-Authenticate']
                    i = server_error_msg.find('error=')
                    if i >= 0:
                        server_error_msg = server_error_msg[i:]
                except KeyError:
                    pass
                if message:
                    message += ' (server said : '+server_error_msg+')'
                else:
                    message = server_error_msg
                return buildCStorageError(response, message)

            # We hack server response code in case of path not found :
            if json and json['errorCode'] == 'ERROR_PATH_NOT_EXIST':
                response.status_code = 404
        except:
            pass
        return buildCStorageError(response, message, c_path)

    def _validate_kanbox_response(self, response, c_path):
        """A response is valid if server code is 2xx. It is recoverable in case of server error 5xx."""
        logger.debug("validating kanbox response: %s %s : %d %s",
                     response.request.method,
                     response.request.url,
                     response.status_code, response.reason)
        code = response.status_code
        if code < 300:
            return response
        if code >= 500:
            # We force connection closing in that case,
            # to avoid being sticked on a non-healthy server :
            response.connection.close()
            raise CRetriableError(self._buildCHttpError(response))
        # Other cases :
        raise self._buildCHttpError(response)

    def _validate_kanbox_api_response(self, response, c_path):
        self._validate_kanbox_response(response)
        # Unfortunately, Kanbox returns a json with Content-Type: text/html !
        # does not work : ensure_content_type_is_json(response, raise_retriable=True)
        # The status is _inside_ payload
        try:
            json = response.json()
        except ValueError:
            raise CRetriableError(self._buildCHttpError(response,
                                                        'Invalid json response : %s' % abbreviate(response.text)))
        if not 'status' in json:
            raise CRetriableError(self._buildCHttpError(response,
                                                        'No status in json response : %s' % abbreviate(response.text)))
        if json['status'] != 'ok':
            raise self._buildCHttpError(response, json)
        return response

    def _get_request_invoker(self, validation_function, c_path):
        session = self._session_manager.get_session()
        request_invoker = RequestInvoker(c_path)
        # We forward directly to request() method :
        request_invoker.do_request = session.request
        request_invoker.validate_response = validation_function
        return request_invoker

    def _get_basic_request_invoker(self, c_path=None):
        return self._get_request_invoker(self._validate_kanbox_response, c_path)

    def _get_api_request_invoker(self, c_path=None):
        return self._get_request_invoker(self._validate_kanbox_api_response, c_path)

    def _get_account_info(self):
        ri = self._get_api_request_invoker()
        json = self._retry_strategy.invoke_retry(ri.get, self.ENDPOINT + '/info').json()
        return json

    def provider_name(self):
        return KanboxStorage.PROVIDER_NAME

    def get_user_id(self):
        """user_id is email in case of kanbox"""
        json = self._get_account_info()
        return json['email']

    def get_quota(self):
        """Return a CQuota object."""
        json = self._get_account_info()
        used = json.get('space_used')  # According to API doc
        if used is None:
            used = json.get('spaceUsed')  # what server actually returns
        allowed = json.get('space_quota')  # According to API doc
        if allowed is None:
            allowed = json.get('spaceQuota')  # what server actually returns
        return CQuota( used, allowed)

    def list_folder(self, c_folder_or_c_path):
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path
        url = KanboxStorage.ENDPOINT + '/list'
        ri = self._get_api_request_invoker(c_path)
        json = self._retry_strategy.invoke_retry(ri.get, url, params={'path': c_path.url_encoded()}).json()
        print(json)

    def upload(self):
        """Upload returns a single char "1" as response"""
        session = self._session_manager.get_session()
        files = {'file': open('/home/lol/temp/xss.png', 'rb')}
        resp = session.post( self.UPLOAD_ENDPOINT + '/test_folder/url.png', files=files)
        print(resp.text)


# TODO is there a simpler way to decorate ?
def _decorate(orig_get_session):
    def get_session_with_hook():
        s = orig_get_session()
        requests_oauthlib.compliance_fixes.kanbox_compliance_fix(s)
        return s
    return get_session_with_hook
