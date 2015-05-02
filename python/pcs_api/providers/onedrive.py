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
#import datetime
import dateutil.parser
import logging
import contextlib
import json
import urllib
#from requests.packages.urllib3.fields import RequestField

from ..storage import IStorageProvider, register_provider
from ..oauth.session_managers import OAuth2SessionManager
from ..oauth.oauth2_params import OAuth2ProviderParameters
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import CRetriableError, CAuthenticationError, CInvalidFileTypeError, CFileNotFoundError, CStorageError
from ..utils import (abbreviate, buildCStorageError, ensure_content_type_is_json, download_data_to_sink, get_content_length)


logger = logging.getLogger(__name__)

@register_provider
class OneDriveStorage(IStorageProvider):
    """FIXME work in progress !
    Some chars are forbidden in file names (see unit tests for details)
    Content-Type is not handled.
    """

    PROVIDER_NAME = 'onedrive'

    # OneDrive endpoint:
    ENDPOINT = 'https://apis.live.net/v5.0'
    ENDPOINT_ME = ENDPOINT + '/me'
    ENDPOINT_ME_SKYDRIVE = ENDPOINT_ME + '/skydrive'

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
        self._root_id = None # lazily retrieved

    def _buildCStorageError(self, response, c_path):
        """FIXME check retriable status codes after calling this method
        """
        error = response.json()['error']
        message = error['code'] + ' (' + error['message'] + ')'
        err = buildCStorageError(response, message, c_path)
        if response.status_code == 420 \
                or (response.status_code >= 500 and response.status_code != 507):
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

    def _build_file_url(self, file_id):
        return self.ENDPOINT + '/' + file_id

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
        url = self.ENDPOINT_ME_SKYDRIVE + '/quota'
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get, url)
        json = resp.json()
        total = json['quota']
        used = total - json['available']
        return CQuota(used, total)

    def list_root_folder(self):
        return self._list_folder_by_id(CPath('/'), self._get_root_id())

    def list_folder(self, c_folder_or_c_path):
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path
        remote_path = self._find_remote_path(c_path)
        if not remote_path.exists():
            # per contract, listing a non existing folder must return None
            return None
        if remote_path.last_is_blob():
            raise CInvalidFileTypeError(c_path, False)
        folder = remote_path.deepest_folder()
        return self._list_folder_by_id(folder.path, folder.file_id)

    def _list_folder_by_id(self, c_path, file_id):
        """
        """
        url = self._build_file_url(file_id) + '/files'
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get, url)
        json = resp.json()
        folder_content = {}
        for val in json['data']:
            val_path = c_path.add(val['name'])
            folder_content[val_path] = self._parse_file(val_path, val)
        return folder_content

    def _get_root_id(self):
        if not self._root_id:
            ri = self._get_api_request_invoker()
            resp = self._retry_strategy.invoke_retry(ri.get, self.ENDPOINT_ME_SKYDRIVE)
            self._root_id = resp.json()['id']
        return self._root_id

    def create_folder(self, c_path):
        # we have to check before if folder already exists:
        # (and also to determine what folders must be created)
        remote_path = self._find_remote_path(c_path)
        if remote_path.last_is_blob():
            # A blob exists along that path: wrong !
            raise CInvalidFileTypeError(remote_path.last_c_path(), False)
        if remote_path.exists():
            # folder already exists:
            return False

        # We may need to create intermediate folders first:
        # FIXME copy/pasted also in upload
        parent_folder = remote_path.deepest_folder()
        i = len(remote_path.files_chain)
        while i < len(remote_path.segments):
            current_c_path = remote_path.first_segments_path(i+1)
            parent_folder = self._raw_create_folder(current_c_path, parent_folder, remote_path.segments[i])
            i += 1
        return True

    class RemotePath(object):
        """Utility class used to convert a CPath to a list of onedrive files ids"""
        def __init__(self, c_path, files_chain, root_id):
            """
            :param c_path : the path (exists or not)
            :param files_chain : tuple of files. If remote c_path exists, len(files_chain) = len(segments).
                               If trailing files do not exist, chain_files list is truncated, and may even be empty.

            Examples : a,b are folders, c.pdf is a blob.
            /a/b/c.pdf --> segments = ('a','b','c.pdf')
                       files_chain = [ {'id':'id_a'...}, {'id':'id_b'...}, {'id':'id_c'...} ]
                       exists() ? True
                       last_is_blob() ? True (c.pdf is not a folder)

            /a/b/c.pdf/d --> segments = ('a','b','c.pdf', 'd')
                       files_chain = [ {'id':'id_a'...}, {'id':'id_b'...}, {'id':'id_c'...} ]
                       exists() ? False
                       last_is_blob() ? True (last is c.pdf)

            In case c.pdf does not exist:
            /a/b/c.pdf --> segments = ('a','b','c.pdf')
                       files_chain = [ {'id':'id_a'...}, {'id':'id_b'...} ]
                       exists() ? False
                       last_is_blob() ? False (if b is folder)
            """
            self.path = c_path
            self.segments = c_path.split()
            self.files_chain = files_chain
            self.root_id = root_id

        def exists(self):
            """Does this path exist onedrive side ?"""
            return len(self.files_chain) == len(self.segments)

        def deepest_folder(self):
            """Return deepest folder in files_chain, or root folder.
            If this remote path does not exist, this is the last existing folder, or root folder.
            Raise ValueError if last segment is a blob"""
            if len(self.files_chain) == 0:
                return CFolder(CPath('/'), self.root_id)
            last = self.files_chain[-1]
            if last.is_folder():
                return last
            # If last is a blob, we return its parent folder:
            if len(self.files_chain) == 1:
                return CFolder(CPath('/'), self.root_id)
            return self.files_chain[-2]

        def deepest_file(self):
            """Return deepest file in files_chain, or root folder."""
            if len(self.files_chain) == 0:
                return CFolder(CPath('/'), self.root_id)
            return self.files_chain[-1]

        def get_blob(self):
            """Return deepest file (should be a blob).
            Raise ValueError if last segment is a blob"""
            if not self.last_is_blob():  # should not happen
                raise ValueError('Inquiring blob of a folder for %r' % self.path)
            return self.files_chain[-1]

        def last_is_blob(self):
            if len(self.files_chain) == 0:
                return False
            return self.files_chain[-1].is_blob()

        def first_segments_path(self, depth):
            """Returns CPath composed of 'depth' first segments"""
            return CPath('/' + '/'.join(self.segments[0:depth]))

        def last_c_path(self):
            """CPath of last existing file"""
            return self.first_segments_path(len(self.files_chain))

    def _find_remote_path(self, c_path):
        """Resolve the given CPath to gather informations (mainly id and type) ; returns a RemotePath object.

        OneDrive API does not allow this natively; we have to start from root folder, and follow the path segments.
        """

        # easy special case:
        if c_path.is_root():
            return OneDriveStorage.RemotePath(c_path, (), self._get_root_id())
        # Here we know that we have at least one path segment

        segments = c_path.split()
        current_path = CPath('/')
        current_id = self._get_root_id()
        files_chain = []
        for i, segment in enumerate(segments):
            content = self._list_folder_by_id(current_path, current_id)
            # locate folder with current segment name:
            current_path = current_path.add(segment)
            next_id = None
            if current_path in content:
                files_chain.append(content[current_path])
                # Stop iteration if we reached a non-folder file:
                if not content[current_path].is_folder():
                    break
                next_id = content[current_path].file_id
            else:
                break
            current_id = next_id
        return OneDriveStorage.RemotePath(c_path, tuple(files_chain), self._get_root_id())

    def _parse_file(self, c_path, json):
        file_id = json['id']
        last_modif = _parse_date_time(json['updated_time'])
        if _is_folder_type(json['type']):
            obj = CFolder(c_path, file_id, last_modif)
        else:
            length = json['size']
            content_type = None  # OneDrive has no content-type...
            obj = CBlob(length, content_type, c_path, file_id, last_modif)
        return obj

    def _raw_create_folder(self, c_path, parent_folder, name):
        ri = self._get_api_request_invoker(c_path)
        url = self._build_file_url(parent_folder.file_id)
        body = {'name': name}
        headers = {'Content-Type': 'application/json'}
        resp = self._retry_strategy.invoke_retry(ri.post, url, data=json.dumps(body), headers=headers)
        return self._parse_file(c_path, resp.json())

    def _delete_by_id(self, c_path, file_id):
        url = self._build_file_url(file_id)
        ri = self._get_api_request_invoker(c_path)
        resp = self._retry_strategy.invoke_retry(ri.delete, url)

    def delete(self, c_path):
        if c_path.is_root():
            raise CStorageError('Can not delete root folder')
        remote_path = self._find_remote_path(c_path)
        if not remote_path.exists():
            # per contract, deleting a non existing folder must return False
            return False
        self._delete_by_id(c_path, remote_path.files_chain[-1].file_id)
        return True

    def get_file(self, c_path):
        """Get CFile for given path, or None if no object exists with that path"""
        if c_path.is_root():
            return CFolder(CPath('/'))
        remote_path = self._find_remote_path(c_path)
        if not remote_path.exists():
            # path does not exist
            return None
        return remote_path.deepest_file()

    def download(self, download_request):
        self._retry_strategy.invoke_retry(self._do_download, download_request)

    def _do_download(self, download_request):
        """This method does NOT retry request"""
        c_path = download_request.path

        remote_path = self._find_remote_path(c_path)
        if not remote_path.exists():
            raise CFileNotFoundError("This file does not exist", c_path)
        if not remote_path.last_is_blob():
            # the path corresponds to a folder
            raise CInvalidFileTypeError(remote_path.last_c_path(), True)

        url = self._build_file_url(remote_path.get_blob().file_id) + '/content'
        headers = download_request.get_http_headers()
        ri = self._get_basic_request_invoker(c_path)
        with contextlib.closing(ri.get(url,
                                       headers=headers,
                                       stream=True)) as response:
            download_data_to_sink(response, download_request.byte_sink())

    def upload(self, upload_request):
        return self._retry_strategy.invoke_retry(self._do_upload, upload_request)

    def _do_upload(self, upload_request):
        c_path = upload_request.path
        base_name = c_path.base_name()
        parent_c_path = c_path.parent()
        remote_path = self._find_remote_path(c_path)
        # Check: is it an existing folder ?
        if remote_path.exists() and not remote_path.last_is_blob():
            # The CPath corresponds to an existing folder, upload is not possible
            raise CInvalidFileTypeError(c_path, True)
        if not remote_path.exists() and remote_path.last_is_blob():
            # some blob exists in path: wrong !
            raise CInvalidFileTypeError(remote_path.last_c_path(), False)

        # Create intermediate folders if required
        # FIXME copy/pasted also in create_folder
        parent_folder = remote_path.deepest_folder()
        i = len(remote_path.files_chain)
        while i < len(remote_path.segments)-1:
            current_c_path = remote_path.first_segments_path(i+1)
            parent_folder = self._raw_create_folder(current_c_path, parent_folder, remote_path.segments[i])
            i += 1

        url = self._build_file_url(parent_folder.file_id) + '/files/' + urllib.quote(base_name.encode('UTF-8'))
        in_stream = upload_request.byte_source().open_stream()
        try:
            ri = self._get_api_request_invoker(c_path)
            resp = ri.put(url=url, data=in_stream)
        finally:
            in_stream.close()


def _parse_date_time(dt_str):
    return dateutil.parser.parse(dt_str)


def _is_blob_type(type):
    """Determine if one drive file type can be represented as a CBlob"""
    return type == 'file' or type == 'photo' or type == 'audio' or type == 'video'


def _is_folder_type(type):
    """Determine if one drive file type can be represented as a CFolder"""
    return type == 'folder' or type == 'album'
