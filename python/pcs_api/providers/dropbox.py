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
import datetime
import dateutil.parser
import logging
import contextlib

from ..storage import IStorageProvider, register_provider
from ..oauth.oauth2_params import OAuth2ProviderParameters
from ..oauth.session_managers import OAuth2SessionManager
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import CRetriableError, CHttpError, CInvalidFileTypeError, CFileNotFoundError, CStorageError
from ..utils import (buildCStorageError, ensure_content_type_is_json, download_data_to_sink)


logger = logging.getLogger(__name__)

@register_provider
class DropboxStorage(IStorageProvider):

    PROVIDER_NAME = 'dropbox'

    # Dropbox endpoints:
    ENDPOINT = 'https://api.dropbox.com/1'
    CONTENT_ENDPOINT = 'https://api-content.dropbox.com/1'

    # OAuth2 endpoints and parameters:
    _oauth2ProviderParameters = OAuth2ProviderParameters(authorize_url=ENDPOINT + '/oauth2/authorize',
                                                         access_token_url=ENDPOINT + '/oauth2/token',
                                                         scope_in_authorization=False)

    def __init__(self, storage_builder):
        self._session_manager = OAuth2SessionManager(self._oauth2ProviderParameters,
                                                     storage_builder.app_info,
                                                     storage_builder.user_creds_repos,
                                                     storage_builder.user_credentials)
        self._scope = storage_builder.app_info.scope[0]  # 'dropbox' or 'sandbox'
        self._retry_strategy = storage_builder.retry_strategy

    def _buildCStorageError(self, response, message, c_path):
        # Try to extract error message from json body:
        server_error_msg = None
        try:
            ct = response.headers['Content-Type']
            if 'text/javascript' in ct or \
               'application/json' in ct:
                json = response.json()
                server_error_msg = json['error']
        except:
            pass
        if server_error_msg:
            if message:
                message += " (server said: " + server_error_msg + ")"
            else:
                message = server_error_msg
        return buildCStorageError(response, message, c_path)

    def _validate_dropbox_api_response(self, response, c_path):
        """Validate a response from dropbox API.

        A response is valid if server code is 2xx and content-type JSON.
        Request is retriable in case of server error 5xx (except 507)."""

        self._validate_dropbox_response(response, c_path)
        # Server response looks correct ; however check content type is json:
        ensure_content_type_is_json(response, raise_retriable=True)
        # OK, response looks fine:
        return response

    def _validate_dropbox_response(self, response, c_path):
        """Validate a response for a file download or API request.

        Only server code is checked (content-type is ignored).
        Request is retriable in case of server error 5xx (except 507)."""
        logger.debug("validating dropbox response: %s %s: %d %s",
                     response.request.method,
                     response.request.url,
                     response.status_code, response.reason)

        if response.status_code == 507: # User is over Dropbox storage quota: no need to retry then
            raise self._buildCStorageError(response, message="Quota exceeded", c_path=c_path)
        if response.status_code >= 500:
            raise CRetriableError(self._buildCStorageError(response, message=None, c_path=c_path))
        if response.status_code >= 300:
            raise self._buildCStorageError(response, message=None, c_path=c_path)
        # OK, response looks fine:
        return response

    def _get_request_invoker(self, validation_function, c_path):
        request_invoker = RequestInvoker(c_path)
        # We forward directly to session manager do_request() method:
        request_invoker.do_request = self._session_manager.do_request
        request_invoker.validate_response = validation_function
        return request_invoker

    def _get_basic_request_invoker(self, c_path=None):
        """An invoker that does not check response content type:
        to be used for files downloading.

        :param c_path: used only to generate CFileNotFoundError if request fails"""
        return self._get_request_invoker(self._validate_dropbox_response, c_path)

    def _get_api_request_invoker(self, c_path=None):
        """An invoker that checks response content type = json:
        to be used by all API requests"""
        return self._get_request_invoker(self._validate_dropbox_api_response, c_path)

    def _parse_c_file_json(self, json):
        if json['is_dir']:
            c_file = CFolder(CPath(json['path']),
                             modification_time=None,
                             metadata=None)
        else:
            c_file = CBlob(json['bytes'], json['mime_type'], CPath(json['path']),
                           modification_time=_parse_date_time(json['modified']),
                           metadata=None)
        return c_file

    def get_url(self, method_path, object_c_path=None):
        """Url encode object path, and concatenate to API endpoint and method to get full URL"""
        if object_c_path is not None:
            return "%s/%s/%s%s" % (self.ENDPOINT, method_path, self._scope, object_c_path.url_encoded())
        else:
            return "%s/%s" % (self.ENDPOINT, method_path)

    def get_content_url(self, method_path, object_c_path):
        """Url encode blob path, and concatenate to content endpoint to get full URL"""
        return "%s/%s/%s%s" % (self.CONTENT_ENDPOINT, method_path, self._scope, object_c_path.url_encoded())

    def get_account_info(self):
        ri = self._get_api_request_invoker()
        url = self.ENDPOINT + '/account/info'
        return self._retry_strategy.invoke_retry(ri.get, url).json()

    def provider_name(self):
        return DropboxStorage.PROVIDER_NAME

    def get_user_id(self):
        """user_id is email in case of dropbox"""
        info = self.get_account_info()
        return info['email']

    def get_quota(self):
        """Return a CQuota object.

        Shared files are counted in used bytes"""
        info = self.get_account_info()
        qinf = info['quota_info']
        return CQuota(qinf['shared'] + qinf['normal'], qinf['quota'])

    def list_root_folder(self):
        return self.list_folder(CPath('/'))

    def list_folder(self, c_folder_or_c_path):
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path
        url = self.get_url('metadata', c_path)
        ri = self._get_api_request_invoker(c_path)
        try:
            response = self._retry_strategy.invoke_retry(ri.get, url)
            json = response.json()
        except CFileNotFoundError:
            # per contract, listing a non existing folder must return None
            return None
        if json.get('is_deleted', False):
            # File is logically deleted
            return None
        if not 'is_dir' in json:
            raise self._buildCStorageError(response, message="No 'is_dir' key in JSON metadata", c_path=c_path)
        if not json['is_dir']:
            raise CInvalidFileTypeError(c_path, False)
        ret = {}
        for f in json['contents']:
            c_file = self._parse_c_file_json(f)
            ret[c_file.path] = c_file
        return ret

    def create_folder(self, c_path):
        cfile = None
        try:
            url = self.get_url('fileops/create_folder')
            ri = self._get_api_request_invoker(c_path)
            self._retry_strategy.invoke_retry(ri.post, url,
                                              params={'root': self._scope, 'path': c_path.path_name()})
            return True
        except CHttpError as che:
            if che.status_code == 403:
                # object already exists: check if real folder or blob:
                cfile = self.get_file(c_path)
                if not cfile.is_folder():
                    # a blob already exists at this path: error !
                    raise CInvalidFileTypeError(c_path, False)
                # Folder already exists:
                return False
            # Other errors are forwarded:
            raise

    def delete(self, c_path):
        if c_path.is_root():
            raise CStorageError('Can not delete root folder')
        url = self.get_url('fileops/delete')
        ri = self._get_api_request_invoker(c_path)
        try:
            json = self._retry_strategy.invoke_retry(ri.post, url,
                                                     params={'root': self._scope, 'path': c_path.path_name()}).json()
        except CFileNotFoundError:
            return False
        return True

    def get_file(self, c_path):
        """Get CFile for given path, or None if no object exists with that path"""
        try:
            url = self.get_url('metadata', c_path)
            ri = self._get_api_request_invoker(c_path)
            json = self._retry_strategy.invoke_retry(ri.get, url, params={'list':False}).json()
            if json.get('is_deleted', False):
                # File is logically deleted
                return None
            return self._parse_c_file_json(json)
        except CFileNotFoundError:
            return None

    def download(self, download_request):
        try:
            return self._retry_strategy.invoke_retry(self._do_download, download_request)
        except CFileNotFoundError:
            # We have to distinguish here between "nothing exists at that path",
            # and "a folder exists at that path":
            c_file = self.get_file(download_request.path)
            if c_file is None:  # Nothing exists
                raise
            elif c_file.is_folder():
                raise CInvalidFileTypeError(c_file.path, True)
            else:
                # Should not happen: a file exists but can not be downloaded ?!
                raise CStorageError('Not downloadable file: %r' % c_file)

    def _do_download(self, download_request):
        """This method does NOT retry request"""
        url = self.get_content_url('files', download_request.path)
        ri = self._get_basic_request_invoker(download_request.path)
        headers = download_request.get_http_headers()
        with contextlib.closing(ri.get(url,
                                       headers=headers,
                                       stream=True)) as response:
            download_data_to_sink(response, download_request.byte_sink())

    def upload(self, upload_request):
        return self._retry_strategy.invoke_retry(self._do_upload, upload_request)

    def _do_upload(self, upload_request):
        # Check before upload: is it a folder ?
        # (uploading a blob to a folder would work, but would rename uploaded file)
        # Would be nice to expect a temporary 100 server response, but requests
        # does not support temporary responses:
        # https://github.com/kennethreitz/requests/issues/713
        c_file = self.get_file(upload_request.path)
        if c_file and c_file.is_folder():
            raise CInvalidFileTypeError(c_file.path, True)

        url = self.get_content_url('files_put', upload_request.path)
        headers = {}
        # Dropbox does not support content-type nor file meta informations:
        #if upload_request._content_type:
        #if upload_request._medatada:
        ri = self._get_basic_request_invoker(upload_request.path)
        in_stream = upload_request.byte_source().open_stream()
        try:
            # httplib does its own loop for sending ;
            ri.put(url, headers=headers, data=in_stream)
        finally:
            in_stream.close()


def _parse_date_time(dt_str):
    """param: dt_str something like "Fri, 07 Mar 2014 17:47:55 +0000"
    """
    return dateutil.parser.parse(dt_str)
