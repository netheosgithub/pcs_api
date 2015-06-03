# -*- coding: utf-8 -*-
#
# Copyright (c) 2015 Netheos (http://www.netheos.net)
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
import dateutil.parser
import logging
import contextlib
import json

from ..storage import IStorageProvider, register_provider
from ..oauth.session_managers import OAuth2SessionManager
from ..oauth.oauth2_params import OAuth2ProviderParameters
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import (CRetriableError, CAuthenticationError, CInvalidFileTypeError,
                           CFileNotFoundError, CStorageError, CHttpError)
from ..utils import (abbreviate, buildCStorageError, ensure_content_type_is_json, download_data_to_sink, get_content_length)


logger = logging.getLogger(__name__)

@register_provider
class OneDriveStorage(IStorageProvider):
    """FIXME work in progress !
    Some chars are forbidden in file names (see unit tests for details)
    Content-Type is not handled.
    See https://github.com/OneDrive/onedrive-api-docs for API reference.
    """

    PROVIDER_NAME = 'onedrive'

    # OneDrive endpoint:
    ENDPOINT = 'https://api.onedrive.com/v1.0'
    ENDPOINT_DRIVE = ENDPOINT + '/drive'  # user default drive
    ENDPOINT_DRIVE_ROOT = ENDPOINT_DRIVE + '/root'
    # This is to retrieve user email (email is user id for user credentials repository)
    ENDPOINT_ME = 'https://apis.live.net/v5.0/me'

    # OAuth2 endpoints and parameters:
    _oauth2ProviderParameters = OAuth2ProviderParameters(
        authorize_url='https://login.live.com/oauth20_authorize.srf',
        access_token_url='https://login.live.com/oauth20_token.srf',
        refresh_token_url='https://login.live.com/oauth20_token.srf',
        scope_in_authorization=True,
        scope_perms_separator=' ')

    def __init__(self, storage_builder):
        self._session_manager = OAuth2SessionManager(self._oauth2ProviderParameters,
                                                     storage_builder.app_info,
                                                     storage_builder.user_creds_repos,
                                                     storage_builder.user_credentials)
        self._scope = storage_builder.app_info.scope
        self._retry_strategy = storage_builder.retry_strategy

    def _buildCStorageError(self, response, c_path):
        """FIXME check retriable status codes after calling this method
        """
        error = response.json()['error']
        message = error['code'] + ' (' + error['message'] + ')'
        err = buildCStorageError(response, message, c_path)
        if response.status_code == 429 \
                or (response.status_code >= 500
                    and response.status_code != 501
                    and response.status_code != 507):
            err = CRetriableError(err)
        return err

    def _validate_onedrive_api_response(self, response, c_path):
        """Validate a response from OneDrive API.

        An API response is valid if response is valid, and content-type is json."""

        self._validate_onedrive_response(response, c_path)
        # Server response looks correct ; however check content type is Json:
        cl = get_content_length(response)
        if cl is not None and cl > 0:
            ensure_content_type_is_json(response, raise_retriable=True)
        # OK, response looks fine:
        return response

    def _validate_onedrive_response(self, response, c_path):
        """Validate a response for a file download or API request.

        Only server code is checked (content-type is ignored)."""
        logger.debug("validating onedrive response: %s %s: %d %s",
                     response.request.method,
                     response.request.url,
                     response.status_code, response.reason)

        if response.status_code >= 300:
            # Determining if error is retriable is not possible without parsing response:
            # FIXME si c'est faisable ici!
            raise self._buildCStorageError(response, c_path)
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
        to be used for files downloading"""
        return self._get_request_invoker(self._validate_onedrive_response, c_path)

    def _get_api_request_invoker(self, c_path=None):
        """An invoker that checks response content type = json:
        to be used by all API requests"""
        return self._get_request_invoker(self._validate_onedrive_api_response, c_path)

    def _build_file_url(self, c_path):
        return self.ENDPOINT_DRIVE_ROOT + ':' + c_path.url_encoded()

    def provider_name(self):
        return OneDriveStorage.PROVIDER_NAME

    def get_user_id(self):
        """user_id is email in case of OneDrive"""
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get, self.ENDPOINT_ME)
        return resp.json()['emails']['account']

    def get_quota(self):
        """Return a CQuota object.
        """
        url = self.ENDPOINT_DRIVE
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get, url)
        quota = resp.json()['quota']
        total = quota['total']
        used = quota['used']
        return CQuota(used, total)

    def list_root_folder(self):
        return self.list_folder(CPath('/'))

    def list_folder(self, c_folder_or_c_path):
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path
        try:
            url = self._build_file_url(c_path) + ":/children"
            ri = self._get_api_request_invoker()
            content_json = self._retry_strategy.invoke_retry(ri.get, url).json()
            #if not remote_path.exists():
            #    # per contract, listing a non existing folder must return None
            #    return None
            #if remote_path.last_is_blob():
            #    raise CInvalidFileTypeError(c_path, False)
            folder_content = {}
            for val in content_json['value']:
                val_path = c_path.add(val['name'])
                folder_content[val_path] = self._parse_item(val_path, val)
            if not folder_content:
                # If we found nothing, it may be a blob ; check it was actually a folder:
                c_file = self.get_file(c_path)
                if c_file.is_blob():
                    raise CInvalidFileTypeError(c_path, False)
            return folder_content
        except CFileNotFoundError as e:
            # Folder does not exist
            return None

    def create_folder(self, c_path):
        if c_path.is_root():
            return False  # we never create the root folder
        try:
            # Intermediate folders are created if they are missing
            ri = self._get_api_request_invoker(c_path)
            url = self._build_file_url(c_path.parent()) + ':/children'
            body = {'name': c_path.base_name(), 'folder': {}}
            headers = {'Content-Type': 'application/json'}
            resp = self._retry_strategy.invoke_retry(ri.post, url, data=json.dumps(body), headers=headers)
            return True
        except CHttpError as e:
            if e.status_code == 409 and e.message.startswith('nameAlreadyExists'):
                # A file already exists ; we have to check it is a folder though
                c_file = self.get_file(c_path)
                if not c_file.is_folder():
                    raise CInvalidFileTypeError(c_path, False)
                return False
            if e.status_code == 403:
                # Most likely a blob exists along the path
                self._raise_if_blob_in_path(c_path)
                raise

    def _parse_item(self, c_path, item_json):
        file_id = item_json['id']
        last_modif = _parse_date_time(item_json['lastModifiedDateTime'])
        if _is_folder_type(item_json):
            obj = CFolder(c_path, file_id, last_modif)
        else:
            length = item_json['size']
            content_type = None  # OneDrive has no content-type...
            obj = CBlob(length, content_type, c_path, file_id, last_modif)
        return obj

    def _raise_if_blob_in_path(self, c_path):
        """Climb up in path hierarchy until we reach a blob, then raise with that blob path.
        If we reach root without ancountering any blob, return normally"""
        while not c_path.is_root():
            c_file = self.get_file(c_path)  # may return None if nothing exists at that path
            if c_file and c_file.is_blob():
                raise CInvalidFileTypeError(c_path, False)
            c_path = c_path.parent()

    def delete(self, c_path):
        if c_path.is_root():
            raise CStorageError('Can not delete root folder')
        url = self._build_file_url(c_path)
        ri = self._get_api_request_invoker(c_path)
        try:
            resp = self._retry_strategy.invoke_retry(ri.delete, url)
            return True
        except CFileNotFoundError as e:
            return False

        #remote_path = self._find_remote_path(c_path)
        #if not remote_path.exists():
        #    # per contract, deleting a non existing folder must return False
        #    return False
        #self._delete_by_id(c_path, remote_path.files_chain[-1].file_id)
        #return True

    def get_file(self, c_path):
        """Get CFile for given path, or None if no object exists with that path"""
        url = self._build_file_url(c_path)
        ri = self._get_api_request_invoker(c_path)
        try:
            resp = self._retry_strategy.invoke_retry(ri.get, url)
            return self._parse_item(c_path, resp.json())
        except CFileNotFoundError as e:
            return None

        if c_path.is_root():
            return CFolder(CPath('/'))
        remote_path = self._find_remote_path(c_path)
        if not remote_path.exists():
            # path does not exist
            return None
        return remote_path.deepest_file()

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
        c_path = download_request.path

        url = self._build_file_url(c_path) + ':/content'
        headers = download_request.get_http_headers()
        ri = self._get_basic_request_invoker(c_path)
        with contextlib.closing(ri.get(url,
                                       headers=headers,
                                       stream=True)) as response:
            download_data_to_sink(response, download_request.byte_sink())

    def upload(self, upload_request):
        c_path = upload_request.path
        self.create_folder(c_path.parent())
        try:
            return self._retry_strategy.invoke_retry(self._do_upload, upload_request)
        except CHttpError as e:
            if e.status_code == 409 and e.message.startswith('nameAlreadyExists'):
                # A file already exists ; most likely a folder
                c_file = self.get_file(c_path)
                if c_file.is_folder():
                    raise CInvalidFileTypeError(c_path, True)
            if e.status_code == 403:
                # Happens when trying to consider blobs as folders along the path
                self._raise_if_blob_in_path(upload_request.path)
            raise

    def _do_upload(self, upload_request):
        """Simple upload for now (limited to 100MB)
        TODO : use resumable upload API"""
        c_path = upload_request.path
        url = self._build_file_url(c_path) + ':/content'
        in_stream = upload_request.byte_source().open_stream()
        try:
            ri = self._get_api_request_invoker(c_path)
            resp = ri.put(url=url, data=in_stream)
        finally:
            in_stream.close()


def _parse_date_time(dt_str):
    return dateutil.parser.parse(dt_str)


def _is_blob_type(item_json):
    """Determine if one drive file type can be represented as a CBlob"""
    return ('file' in item_json
            or 'photo' in item_json
            or 'audio' in item_json
            or 'video' in item_json)


def _is_folder_type(item_json):
    """Determine if one drive file type can be represented as a CFolder"""
    return ('folder' in item_json
            or 'album' in item_json)
