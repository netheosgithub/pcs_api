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
import requests
import urllib
import logging
import datetime
import dateutil
import contextlib

from ..storage import IStorageProvider, register_provider
from ..oauth.session_managers import BasicAuthSessionManager
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import CRetriableError, CInvalidFileTypeError, CFileNotFoundError, CStorageError
from ..utils import (buildCStorageError, ensure_content_type_is_json, shorten_url,
                     abbreviate, download_data_to_sink, get_content_length, MultipartEncoder)

logger = logging.getLogger(__name__)


@register_provider
class RapidShareStorage(IStorageProvider):
    """
    Folder names are not limited (may contain a comma listed as %2C in listrealfolders, etc. even \ and / are accepted).
    However for files (blobs) names, some chars are changed by rapidshare and replaced by _ (underscore). Several blobs
    with the same name (but different ids) may thus be created: for example foo, and foo" will both be stored as foo_
    Forbidden chars are: ' " , < > \
    Rapidshare works with ids rather than paths. ids must therefore be retrieved from path, thanks to
    API call 'listrealfolders'.
    Folders names should NOT contain / or \ characters or pcs_api will fail.

    Warning: because files are handled by id, rapidshare tolerates duplicates in folder names and blobs names.
    pcs_api tries to avoid such duplicates, but may fail to ensure unicity in case of multithreaded accesses.
    Any existing blob with same name is deleted before upload (so if upload fails, old file is lost).

    Files uploads: the limit is 100MB for uploads with no account and 2 GB with account.
    Requests rate limit: account locked if more than 6500 API calls in less then 10 minutes.
    """

    PROVIDER_NAME = 'rapidshare'

    ENDPOINT = 'https://api.rapidshare.com/cgi-bin/rsapi.cgi'
    # TODO: put that in a configuration
    MAX_API_RESPONSE_LENGTH = 1024*1024

    def __init__(self, storage_builder):
        self._session_manager = LoginPasswordParamsSessionManager(storage_builder.user_credentials)
        self._retry_strategy = storage_builder.retry_strategy

    def _buildCStorageError(self, response, message, c_path):
        # Try to extract error message from first line of server response body:
        server_error_msg = None
        try:
            if response.text.startswith('ERROR: '):
                # Abbreviate message if too long:
                server_error_msg = abbreviate(response.text)
                # FIXME change that !
                if 'Login failed' in server_error_msg:
                    # Here we change server status code: should really be a 401 !
                    response.status_code = 401
                elif 'File not found' in server_error_msg:
                    # Here we change server status code: should really be a 404 !
                    response.status_code = 404
        except:
            pass
        if server_error_msg:
            if message:
                message += " (server said: " + server_error_msg + ")"
            else:
                message = server_error_msg
        return buildCStorageError(response, message, c_path)

    def _validate_rapidshare_api_response(self, response, c_path):
        """Validate a response from rapidshare API.

        A response is valid if server code is 2xx and content does not start with "ERROR:".
        Request is retriable in case of server error 5xx."""

        self._validate_rapidshare_response(response, c_path)
        # Let's protect ourselves if response is too big: (rapidshare always sends Content-Length header)
        length = get_content_length(response)
        if length is None:
            raise CRetriableError(self._buildCStorageError(response,
                                                           "Undefined content length in server API response",
                                                           c_path))
        if length > RapidShareStorage.MAX_API_RESPONSE_LENGTH:
            raise self._buildCStorageError(response, "Too large server API response (%d bytes)" % length, c_path)
        if response.text.startswith('ERROR: '):
            raise self._buildCStorageError(response, None, c_path)
        # OK, response looks correct:
        return response

    def _validate_rapidshare_response(self, response, c_path):
        """Validate a response for a file download or API request.

        Only server code is checked (content-type is ignored).
        Request is retriable in case of server error 5xx."""
        logger.debug("validating rapidshare response: %s %s: %d %s",
                     response.request.method,
                     shorten_url(response.request.url),
                     response.status_code, response.reason)

        if response.status_code >= 500:
            raise CRetriableError(self._buildCStorageError(response, None, c_path))
        if response.status_code >= 300:
            raise self._buildCStorageError(response, None, c_path)
        return response

    def _get_request_invoker(self, validation_function):
        session = self._session_manager.get_session()
        request_invoker = RequestInvoker()
        # We forward directly to request() method:
        request_invoker.do_request = session.request
        request_invoker.validate_response = validation_function
        return request_invoker

    def _get_basic_request_invoker(self):
        """An invoker that does not check response content type:
        to be used for files downloading"""
        return self._get_request_invoker(self._validate_rapidshare_response)

    def _get_api_request_invoker(self):
        """An invoker to be used by all API requests"""
        return self._get_request_invoker(self._validate_rapidshare_api_response)

    def provider_name(self):
        return RapidShareStorage.PROVIDER_NAME

    def get_user_id(self):
        return self._session_manager._user_credentials.user_id

    def _get_account_details(self):
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get,
                                                 url=RapidShareStorage.ENDPOINT,
                                                 params={'sub': 'getaccountdetails'}).text
        # Parse lines ; each line has format key=value
        # We'll return a map: BEWARE that some keys may change over time !
        ret = {}
        for line in resp.splitlines():
            key, value = line.split('=', 1)
            ret[key] = value
        return ret

    def get_quota(self):
        """Return a CQuota object.

        Shared files are counted in used bytes"""
        info = self._get_account_details()
        try:
            used_bytes = int(info['curspace'])
        except KeyError:
            used_bytes = -1
        try:
            allowed_bytes = int(info['maxspacegb']) * 1024*1024*1024
        except KeyError:
            allowed_bytes = -1
        return CQuota(used_bytes, allowed_bytes)

    def _get_all_folders_and_parent_ids(self):
        """Request API to list ALL user folders (not blobs): required to convert path to id back & forth.

        Returns a list of tuples: id, parent_id, basename (ids are int)"""
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get,
                                                 url=RapidShareStorage.ENDPOINT,
                                                 params={'sub': 'listrealfolders'}).text
        if resp == 'NONE':
            # special case when no result should be returned:
            return []

        # parse lines: id,parent id, name, ... (ACLs not parsed) and put that into a temp list:
        files = []
        for line in resp.splitlines():
            id_str, parent_id_str, basename = line.split(',')[0:3]
            # we have to unescape path: %2C, etc.(% itself is always escaped as %25)
            basename = urllib.unquote(basename)  # already unicode
            # convert id to integers so that they will not be confuzed with pathnames:
            files.append((int(id_str), int(parent_id_str), basename))
        return files

    def _get_folders_ids_map(self, folders_and_parent_ids):
        """
        :param folders_and_parent_ids: list of tuples, as returned by _get_all_folders_and_parent_ids()
        :return a dict: key=full pathname (string), value=id (int)
                        and also key=id (int), value=pathname (string)."""

        # We go through the list to build the map:
        # rapidshare usually returns parents first, so this should be quick
        # but this is not guaranteed, so we may have to loop
        ret = {0: '', '': 0}  # avoids double slashs // in loop below
        done = False
        changed = True
        while not done and changed:
            done = True
            changed = False
            for file_id_int, file_parent_id_int, basename in folders_and_parent_ids:
                if file_id_int in ret:
                    continue
                if not file_parent_id_int in ret:
                    done = False
                    continue  # no known parent yet, we'll have to loop again
                    # add link:
                parent_path = ret[file_parent_id_int]
                pathname = parent_path + '/' + basename
                ret[pathname] = file_id_int
                ret[file_id_int] = pathname
                changed = True
        if not done:
            logger.error("Could not connect all folders by ids")
        # normalize root path:
        del ret['']
        ret['/'] = 0
        ret[0] = '/'
        return ret

    def _get_folders_by_parent_id(self, parent_id, folders_and_parent_ids):
        """Retrieve folders in the given folder given by its id.
        Search is done in list folders_and_parent_ids (no request is performed).

        :return list of tuples folder_id, folder_basename"""
        ret = []
        for folder_id, folder_parent_id, basename in folders_and_parent_ids:
            if parent_id == folder_parent_id:
                ret.append((folder_id, basename))
        return ret

    def _list_blobs_by_parent_id(self, c_path, parent_folder_id, search_filter=None):
        """Retrieve map of blobs in a folder given by its id.
        Optionally filter returned map to contain only given file"""
        ri = self._get_api_request_invoker()
        params = {'sub': 'listfiles',
                  'realfolder': parent_folder_id,
                  'fields': 'filename,size,type,uploadtime'}
        if search_filter:
            params['filename'] = search_filter
        resp = self._retry_strategy.invoke_retry(ri.get,
                                                 url=RapidShareStorage.ENDPOINT,
                                                 params=params).text
        ret = {}
        if resp == 'NONE':
            # special case when no result should be returned:
            return ret
        for line in resp.splitlines():
            # this print is useful when debugging problems special chars filenames:
            #print("RS responded: ", line)
            id_str, basename, size_str, type_str, upload_str = line.split(',')
            file_c_path = c_path.add(basename)
            ret[file_c_path] = CBlob(int(size_str),
                                     None,  # content-type
                                     file_c_path,
                                     id_str,  # file_id is a unicode string
                                     datetime.datetime.fromtimestamp(int(upload_str), tz=dateutil.tz.tzutc()))
        return ret

    def list_root_folder(self):
        return self.list_folder(CPath('/'))

    def list_folder(self, c_folder_or_c_path):
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path
        folders_and_parents_ids = self._get_all_folders_and_parent_ids()
        folders_ids_map = self._get_folders_ids_map(folders_and_parents_ids)
        if not c_path.path_name() in folders_ids_map:
            # Path is not a folder
            # We have to distinguish between non existing (return None) and blob (raise CInvalidFileType):
            parent_path = c_path.parent()
            if not parent_path.path_name() in folders_ids_map:
                # parent folder does not even exist
                return None
            parent_id = folders_ids_map[parent_path.path_name()]
            blobs = self._list_blobs_by_parent_id(parent_path, parent_id, search_filter=c_path.base_name())
            if c_path in blobs:
                raise CInvalidFileTypeError(c_path, False)
            return None

        id = folders_ids_map[c_path.path_name()]
        ret = {}
        # Add sub-folders:
        for sub_folder_id, sub_folder_name in self._get_folders_by_parent_id(id, folders_and_parents_ids):
            sub_path = CPath(folders_ids_map[sub_folder_id])
            ret[sub_path] = CFolder(sub_path)

        # Now for blobs:
        blobs = self._list_blobs_by_parent_id(c_path, id)
        ret.update(blobs)
        return ret

    def _raw_create_folder(self, folder_c_path, parent_id):
        """Create a folder without creating any higher level intermediary folders,
        and returned id of created folder."""
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get,
                                                 url=RapidShareStorage.ENDPOINT,
                                                 params={'sub': 'addrealfolder',
                                                         'name': folder_c_path.base_name(),
                                                         'parent': parent_id})
        return int(resp.content)

    def _create_intermediary_folders_objects(self, leaf_folder_path, folders_ids_map):
        """Create any parent folders if they do not exist.

        As an optimization, we consider that if folder a/b/c exists, then a/ and a/b/ also exist
        so are not checked nor created.
        :returns leaf_folder id"""
        # We check for folder existence before creation,
        # as in general leaf folder is likely to already exist.
        # So we walk from leaf to root:
        c_path = leaf_folder_path
        parent_folders = []
        while not c_path.is_root():  # deepest first
            if c_path.path_name() in folders_ids_map:
                break
            logger.debug("Nothing exists at path: %s, will go up", c_path)
            parent_folders.insert(0, c_path)
            c_path = c_path.parent()
        # By now we know which folders to create:
        if parent_folders:
            # we have to check that the first folder to create is not a blob:
            parent_id = folders_ids_map[c_path.path_name()]
            c_files = self._list_blobs_by_parent_id(c_path, parent_id, parent_folders[0].base_name())
            if parent_folders[0] in c_files:
                # Problem here: clash between folder and blob
                raise CInvalidFileTypeError(parent_folders[0], False)
            logger.debug("Inexisting parent_folders will be created: %r", parent_folders)
            for c_path in parent_folders:
                logger.debug("Creating intermediary folder: %r", c_path)
                parent_id = self._raw_create_folder(c_path, parent_id)
        else:
            # nothing to create, leaf folder already exists:
            parent_id = folders_ids_map[leaf_folder_path.path_name()]
        return parent_id

    def create_folder(self, c_path):
        # folder may already exist: we check to avoid duplicates
        folders_and_parents_ids = self._get_all_folders_and_parent_ids()
        folders_ids_map = self._get_folders_ids_map(folders_and_parents_ids)
        if c_path.path_name() in folders_ids_map:
            # already exists
            return False
        # Folder does not exist: we'll create it but we may need to create intermediary folders as well
        self._create_intermediary_folders_objects(c_path, folders_ids_map)
        return True

    def delete(self, c_path):
        if c_path.is_root():
            raise CStorageError('Can not delete root folder')
        # We have to delete folders recursively, to avoid orphans

        folders_and_parents_ids = self._get_all_folders_and_parent_ids()
        folders_ids_map = self._get_folders_ids_map(folders_and_parents_ids)
        # Filter and sort all sub-folders of c_path:
        # The sort ensure deepest folders will be deleted first
        cpn = c_path.path_name()
        pathnames = filter(lambda pn: isinstance(pn, unicode) and (pn == cpn or pn.startswith(cpn+'/')),
                           folders_ids_map.keys())
        # Delete folders one by one:
        pathnames = sorted(pathnames, reverse=True)
        for pn in pathnames:  # may be empty
            # delete folder:
            folder_id = folders_ids_map[pn]
            ri = self._get_api_request_invoker()
            resp = self._retry_strategy.invoke_retry(ri.get,
                                                     url=RapidShareStorage.ENDPOINT,
                                                     params={'sub': 'delrealfolder', 'realfolder':folder_id})
        if pathnames:
            return True  # We deleted at least one folder

        # Not a folder ; is it a blob ?
        parent_path = c_path.parent()
        try:
            parent_id = folders_ids_map[parent_path.path_name()]
        except KeyError:
            # Parent folder does not exist: then nothing to delete
            return None
        # Now list parent folder (we restrict returned list)
        blobs = self._list_blobs_by_parent_id(parent_path, parent_id, search_filter=c_path.base_name())
        if not c_path in blobs:
            # Nothing exists
            return False

        # It is a blob: delete
        self._delete_blob_by_id(blobs[c_path].file_id)
        return True

    def _delete_blob_by_id(self, blob_id):
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get,
                                                 url=RapidShareStorage.ENDPOINT,
                                                 params={'sub':'deletefiles', 'files': blob_id})
        # TODO check response ?

    def get_file(self, c_path):
        # speed up if root folder:
        if c_path.is_root():
            return CFolder(c_path)
        # is it a folder ? search in all realfolders
        folders_and_parents_ids = self._get_all_folders_and_parent_ids()
        folders_ids_map = self._get_folders_ids_map(folders_and_parents_ids)
        if c_path.path_name() in folders_ids_map:
            # It is a folder:
            return CFolder(c_path)

        # Not found: it may not exist, or it may be a blob
        # What about parent folder ?
        parent_path = c_path.parent()
        try:
            parent_id = folders_ids_map[parent_path.path_name()]
        except KeyError:
            # Parent folder does not exist: then file does not exist
            return None
        # Now list parent folder (we restrict returned list)
        blobs = self._list_blobs_by_parent_id(parent_path, parent_id, search_filter=c_path.base_name())
        if c_path in blobs:
            # It is a blob:
            return blobs[c_path]
        return None

    def download(self, download_request):
        c_path = download_request.path
        folders_and_parents_ids = self._get_all_folders_and_parent_ids()
        folders_ids_map = self._get_folders_ids_map(folders_and_parents_ids)
        if c_path.path_name() in folders_ids_map:
            # It is folder !
            raise CInvalidFileTypeError(download_request.path, True)
        # Retrieve blob id:
        parent_path = c_path.parent()
        if not parent_path.path_name() in folders_ids_map:
            raise CFileNotFoundError('Parent path not found: %r' % parent_path, c_path)
        parent_id = folders_ids_map[parent_path.path_name()]
        blobs = self._list_blobs_by_parent_id(parent_path, parent_id, c_path.base_name())
        try:
            blob = blobs[c_path]
        except KeyError:
            raise CFileNotFoundError('File not found: %r' % c_path, c_path)

        return self._retry_strategy.invoke_retry(self._do_download, download_request, blob.file_id)

    def _do_download(self, download_request, blob_id):
        c_path = download_request.path
        # Determine URL to download from:
        ri = self._get_api_request_invoker()
        resp = ri.get(url=RapidShareStorage.ENDPOINT,
                      params={'sub': 'download',
                              'fileid': blob_id,
                              'filename': c_path.base_name(),
                              'try': '1'})  # so that we always get final URL
        if not resp.text.startswith('DL:'):
            e = self._buildCHttpError(resp, 'Can not download (response body does not start with DL:)')
            raise CRetriableError(e)  # try again
        hostname = resp.text[3:].split(',')[0]
        if not hostname.endswith('.rapidshare.com'):
            raise self._buildCHttpError(resp, "Download host is not rapidshare.com: %r" % hostname)
        ri = self._get_basic_request_invoker()
        url = 'https://%s/files/%s/%s' % (hostname, blob_id, urllib.quote(c_path.base_name().encode('UTF-8')))
        headers = download_request.get_http_headers()
        with contextlib.closing(ri.get(url=url,
                                       headers=headers,
                                       params={'directstart': '1'},
                                       stream=True)) as response:
            download_data_to_sink(response, download_request.byte_sink())

    def upload(self, upload_request):
        # Check before upload: is it a folder ?
        # (uploading a blob to a folder would work, but would create a duplicate in names)
        c_path = upload_request.path
        folders_and_parents_ids = self._get_all_folders_and_parent_ids()
        folders_ids_map = self._get_folders_ids_map(folders_and_parents_ids)
        if c_path.path_name() in folders_ids_map:
            # It is folder !
            raise CInvalidFileTypeError(c_path, True)
        # We may need to create intermediary folders:
        parent_path = c_path.parent()
        parent_id = self._create_intermediary_folders_objects(parent_path, folders_ids_map)

        # If blob already exists with the same name ; if yes, delete it first to avoid duplicates
        blobs = self._list_blobs_by_parent_id(parent_path, parent_id, search_filter=c_path.base_name())
        if c_path in blobs:
            self._delete_blob_by_id(blobs[c_path].file_id)
        # OK for upload:
        return self._retry_strategy.invoke_retry(self._do_upload, upload_request, parent_id)

    def _do_upload(self, upload_request, parent_id):
        # At first find next upload server:
        ri = self._get_api_request_invoker()
        resp = self._retry_strategy.invoke_retry(ri.get, url=RapidShareStorage.ENDPOINT,
                                                 params={'sub': 'nextuploadserver'})
        upload_number = int(resp.text)
        hostname = 'rs%d.rapidshare.com' % upload_number

        # Next do upload:
        url = 'https://%s/cgi-bin/rsapi.cgi' % hostname
        # Rapidshare does not support content-type nor file meta informations:
        #if upload_request._content_type:
        #if upload_request._medatada:
        ri = self._get_api_request_invoker()
        in_stream = upload_request.byte_source().open_stream()
        parts = self._session_manager.get_authentication_parts()  # if we post, user/pw must be in post
        parts.update({'sub': 'upload',
                      'folder': str(parent_id),
                      'filename': upload_request.path.base_name(),
                      'filecontent': in_stream})
        encoder = MultipartEncoder(parts)
        try:
            ri.post(url,
                    headers={'Content-Type': encoder.content_type},  # multipart/form-data
                    data=encoder)
        finally:
            in_stream.close()


class LoginPasswordParamsSessionManager(BasicAuthSessionManager):
    """Special rapidshare authentication: login & password are passed as additional query parameters
    in case of GET requests, and in form data in case of POST requests."""

    def __init__(self, user_credentials):
        super(LoginPasswordParamsSessionManager, self).__init__(user_credentials)

    def get_session(self):
        session = requests.Session()
        session.params.update({"login": self._user_credentials.user_id,
                               "password": self._user_credentials.credentials().get('password')})
        return session

    def get_authentication_parts(self):
        return { 'login': self._user_credentials.user_id,
                 'password': self._user_credentials.credentials()['password'] }


