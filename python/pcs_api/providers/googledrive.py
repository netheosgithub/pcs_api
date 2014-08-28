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
import json
from requests.packages.urllib3.fields import RequestField

from ..storage import IStorageProvider, register_provider
from ..oauth.oauth2_params import OAuth2ProviderParameters
from ..oauth.session_managers import OAuth2SessionManager
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import CRetriableError, CAuthenticationError, CInvalidFileTypeError, CFileNotFoundError, CStorageError
from ..utils import (buildCStorageError, ensure_content_type_is_json, download_data_to_sink, MultipartEncoder)


logger = logging.getLogger(__name__)

@register_provider
class GoogleDriveStorage(IStorageProvider):
    """GoogleDrive provider.

    Note that OAuth2 refresh token is returned by oauth endpoint only if user approves an offline access. This is the
    purpose of query parameters "access_type=offline&approval_prompt=force" in authorizeUrl. Beware that old refresh
    tokens may be invalidated by such requests though: see https://developers.google.com/accounts/docs/OAuth2
    """

    PROVIDER_NAME = 'googledrive'

    # Google endpoints:
    ENDPOINT = 'https://www.googleapis.com/drive/v2'
    FILES_ENDPOINT = ENDPOINT + '/files'
    FILES_UPLOAD_ENDPOINT = 'https://www.googleapis.com/upload/drive/v2/files'
    USERINFO_ENDPOINT = 'https://www.googleapis.com/oauth2/v1/userinfo'
    MIME_TYPE_DIRECTORY = 'application/vnd.google-apps.folder'

    # OAuth2 endpoints and parameters:
    _oauth2ProviderParameters = OAuth2ProviderParameters(
        authorize_url='https://accounts.google.com/o/oauth2/auth?access_type=offline&approval_prompt=force',
        access_token_url='https://accounts.google.com/o/oauth2/token',
        refresh_token_url='https://accounts.google.com/o/oauth2/token',
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
        # Try to extract error message from json body:
        message = None
        try:
            ct = response.headers['Content-Type']
            if 'text/javascript' in ct or 'application/json' in ct:
                error = response.json()['error']
                jcode = error['code']
                jreason = error['errors'][0]['reason']
                message = '[%d/%s] ' % (jcode, jreason)
                message += error['message']
                if jcode == 403 and jreason == 'userAccess':
                    # permission error: indicate failing path helps:
                    message += ' (%r)' % c_path
        except:
            logger.warn('Unparsable server error message: %s', response.text)
        return buildCStorageError(response, message, c_path)

    def _validate_drive_api_response(self, response, c_path):
        """Validate a response from google drive API.

        An API response is valid if response is valid, and content-type is JSON."""

        self._validate_drive_response(response, c_path)
        # Server response looks correct ; however check content type is json:
        ensure_content_type_is_json(response, raise_retriable=True)
        # OK, response looks fine:
        return response

    def _validate_drive_response(self, response, c_path):
        """Validate a response for a file download or API request.

        Only server code is checked (content-type is ignored).
        Request is retriable in case of server error 5xx or some 403 errors with rate limit."""
        logger.debug("validating googledrive response: %s %s: %d %s",
                     response.request.method,
                     response.request.url,
                     response.status_code, response.reason)

        if response.status_code >= 300:
            cse = self._buildCStorageError(response, c_path)
            if response.status_code >= 500:
                raise CRetriableError(cse)
            # Some 403 errors (rate limit) may be retriable:
            if (response.status_code == 403
                and cse.message
                and (cse.message.startswith('[403/rateLimitExceeded]')
                     or cse.message.startswith('[403/userRateLimitExceeded]'))):
                raise CRetriableError(cse)
            # other errors are not retriable:
            raise cse
        # OK, response looks fine:
        return response

    def _get_request_invoker(self, validation_function, c_path):
        request_invoker = GoogleDriveRequestInvoker(self._session_manager, validation_function, c_path)
        return request_invoker

    def _get_basic_request_invoker(self, c_path=None):
        """An invoker that does not check response content type:
        to be used for files downloading"""
        return self._get_request_invoker(self._validate_drive_response, c_path)

    def _get_api_request_invoker(self, c_path=None):
        """An invoker that checks response content type = json:
        to be used by all API requests"""
        return self._get_request_invoker(self._validate_drive_api_response, c_path)

    def _parse_c_file_json(self, parent_c_path, json):
        if json['mimeType'] == self.MIME_TYPE_DIRECTORY:
            c_file = CFolder(parent_c_path.add(json['title']),
                             modification_time=_parse_date_time(json['modifiedDate']),
                             metadata=None)
        else:
            try:
                file_size = int(json['fileSize'])
            except KeyError:
                # google apps files (application/vnd.google-apps.document, application/vnd.google-apps.spreadsheet, etc.)
                # do not publish any size (they can not be downloaded, only exported).
                file_size = -1  # FIXME what else ? Or should we hide these kind of "blob" that are actually not downloadable ?
            c_file = CBlob(file_size, json['mimeType'], parent_c_path.add(json['title']),
                           modification_time=_parse_date_time(json['modifiedDate']),
                           metadata=None)
        return c_file

    def _get_file_url(self, file_id):
        return "%s/files/%s" % (self.ENDPOINT, file_id)

    class RemotePath(object):
        """Utility class used to convert a CPath to a list of google drive files ids"""
        def __init__(self, c_path, files_chain):
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

        def exists(self):
            """Does this path exist google side ?"""
            return len(self.files_chain) == len(self.segments)

        def deepest_folder_id(self):
            """Return id of deepest folder in files_chain, or 'root'.
            If this remote path does not exist, this is the last existing id, or 'root'.
            Raise ValueError if last segment is a blob"""
            if len(self.files_chain) == 0:
                return 'root'
            last = self.files_chain[-1]
            if last['mimeType'] == GoogleDriveStorage.MIME_TYPE_DIRECTORY:
                return last['id']
            # If last is a blob, we return the parent id:
            if len(self.files_chain) == 1:
                return 'root'
            return self.files_chain[-2]['id']

        def get_blob(self):
            """Return deepest object (should be a blob).
            Object attributes are 'id', 'mimeType', 'downloadUrl'
            Raise ValueError if last segment is a blob"""
            if not self.last_is_blob():  # should not happen
                raise ValueError('Inquiring blob of a folder for %r' % self.path)
            return self.files_chain[-1]

        def last_is_blob(self):
            return (len(self.files_chain) > 0
                    and self.files_chain[-1]['mimeType'] != GoogleDriveStorage.MIME_TYPE_DIRECTORY)

        def first_segments_path(self, depth):
            """Returns CPath composed of 'depth' first segments"""
            return CPath('/' + '/'.join(self.segments[0:depth]))

        def last_c_path(self):
            """CPath of last existing file"""
            return self.first_segments_path(len(self.files_chain))

    def _find_remote_path(self, c_path, detailed=False):
        """Resolve the given CPath to gather informations (mainly id and mimeType) ; returns a RemotePath object.

        Drive API does not allow this natively ; we perform a single request that returns
        all files (but may return too much): find files with title='a' or title='b' or title='c',
        then we connect children and parents to get the chain of ids.
        TODO This fails if there are several folders with same name, and we follow the wrong branch"""

        # easy special case:
        if c_path.is_root():
            return GoogleDriveStorage.RemotePath(c_path, ())
        # Here we know that we have at least one path segment

        # Build query (cf. https://developers.google.com/drive/web/search-parameters)
        segments = c_path.split()
        q = '('
        for i, segment in enumerate(segments):
            if i > 0:
                q += ' or '
            q += "(title='%s'" % segment.replace("'", "\\'")  # escape ' --> \'
            # for all but last segment, we enforce file to be a directory
            # TODO this creates looong query string, is that interesting ?
            #if i < len(segments)-1:
            #    q += " and mimeType='%s'" % self.MIME_TYPE_DIRECTORY
            q += ')'
        q += ') and trashed = false'
        #print("segments = ",segments)
        #print("query = ",q)


        # drive may not return all results in a single query:
        # FIXME ouch there seems to be some issues with pagination on the google side ?
        # http://stackoverflow.com/questions/18646004/drive-api-files-list-query-with-not-parameter-returns-empty-pages?rq=1
        # http://stackoverflow.com/questions/18355113/paging-in-files-list-returns-endless-number-of-empty-pages?rq=1
        # http://stackoverflow.com/questions/19679190/is-paging-broken-in-drive?rq=1
        # http://stackoverflow.com/questions/16186264/files-list-reproducibly-returns-incomplete-list-in-drive-files-scope
        items = []
        next_page_token = None
        while True:
            # Execute request ; we ask for specific fields only
            fields_filter = 'id,title,mimeType,parents/id,parents/isRoot'
            if detailed:
                fields_filter += ',downloadUrl,modifiedDate,fileSize'
            fields_filter = 'nextPageToken,items(' + fields_filter + ')'
            ri = self._get_api_request_invoker()
            resp = self._retry_strategy.invoke_retry(ri.get,
                                                     url=GoogleDriveStorage.FILES_ENDPOINT,
                                                     params={'q': q,
                                                             'fields': fields_filter,
                                                             'pageToken': next_page_token,
                                                             'maxResults': 100})
            #print(resp.text)
            jresp = resp.json()
            items.extend(jresp['items'])
            try:
                next_page_token = jresp['nextPageToken']
                logger.debug('_find_remote_path() will loop: (%d items in this page)', len(jresp['items']))
            except KeyError:
                logger.debug('_find_remote_path(): no more data for this query')
                break

        # Now connect parent/children to build the path:
        files_chain = []
        for i, searched_segment in enumerate(segments):
            first_segment = (i == 0)  # this changes parent condition (isRoot, or no parent for shares)
            last_segment = (i == len(segments)-1)
            #print("searching segment ",searched_segment)
            next_item = None
            for item in items:
                #print("examaning item=",item)
                # We match title
                # FIXME and enforce type is directory if not last segment:
                if (item['title'] == searched_segment):
                    #and (last_segment or item['mimeType'] == self.MIME_TYPE_DIRECTORY)):
                    parents = item['parents']
                    if first_segment:
                        if not parents:  # no parents (shared folder ?)
                            next_item = item
                            break
                        for p in parents:
                            if p['isRoot']:  # at least one parent is root
                                next_item = item
                                break
                    else:
                        for p in parents:
                            if p['id'] == files_chain[-1]['id']:  # at least one parent id is last parent id
                                next_item = item
                                break
                    if next_item:
                        break
            if not next_item:
                break
            files_chain.append(next_item)
        return GoogleDriveStorage.RemotePath(c_path, tuple(files_chain))

    def provider_name(self):
        return GoogleDriveStorage.PROVIDER_NAME

    def get_user_id(self):
        """user_id is email in case of googledrive"""
        ri = self._get_api_request_invoker()
        url = self.USERINFO_ENDPOINT
        info = self._retry_strategy.invoke_retry(ri.get, url).json()
        return info['email']

    def get_quota(self):
        """Return a CQuota object.

        Shared files are not counted in used bytes."""
        ri = self._get_api_request_invoker()
        url = self.ENDPOINT + '/about'
        about = self._retry_strategy.invoke_retry(ri.get, url).json()
        #print(about)
        return CQuota(int(about['quotaBytesUsed']), int(about['quotaBytesTotal']))

    def list_root_folder(self):
        return self.list_folder(CPath('/'))

    def list_folder(self, c_folder_or_c_path):
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path
        remote_path = self._find_remote_path(c_path, detailed=True)
        if not remote_path.exists():
            # per contract, listing a non existing folder must return None
            return None
        if remote_path.last_is_blob():
            raise CInvalidFileTypeError(c_path, False)

        # Now we inquire for children of leaf folder:
        folder_id = remote_path.deepest_folder_id()
        url = self.FILES_ENDPOINT
        ri = self._get_api_request_invoker()
        q = "('%s' in parents" % folder_id
        if c_path.is_root():
            # If we list root folder, also list shared files, as they appear here:
            q += ' or sharedWithMe'
        q += ") and trashed=false"
        fields_filter = 'nextPageToken,items(id,title,mimeType,fileSize,modifiedDate)'
        response = self._retry_strategy.invoke_retry(ri.get, url,
                                                     params={'q': q, 'fields': fields_filter})
        #print(response.text)
        jresp = response.json()
        ret = {}
        for item in jresp['items']:
            c_file = self._parse_c_file_json(c_path, item)
            ret[c_file.path] = c_file

        return ret

    def _raw_create_folder(self, c_path, parent_id):
        """Create a folder without creating any higher level intermediary folders,
        and returned id of created folder."""
        url = self.FILES_ENDPOINT
        ri = self._get_api_request_invoker(c_path)
        body = {'title': c_path.base_name(),
                'mimeType': self.MIME_TYPE_DIRECTORY,
                'parents': [{'id': parent_id}]}
        resp = self._retry_strategy.invoke_retry(ri.post, url,
                                                 data=json.dumps(body),
                                                 headers={'Content-type': 'application/json'},
                                                 params={'fields': 'id'})
        jresp = resp.json()
        parent_id = jresp['id']
        return parent_id

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

        # we may have to create any intermediary folders:
        parent_id = remote_path.deepest_folder_id()
        i = len(remote_path.files_chain)
        while i < len(remote_path.segments):
            current_c_path = remote_path.first_segments_path(i+1)
            parent_id = self._raw_create_folder(current_c_path, parent_id)
            i += 1
        return True

    def _delete_by_id(self, c_path, file_id):
        url = self._get_file_url(file_id) + '/trash'
        ri = self._get_api_request_invoker(c_path)
        resp = self._retry_strategy.invoke_retry(ri.post, url)

    def delete(self, c_path):
        """Move file to trash"""
        if c_path.is_root():
            raise CStorageError('Can not delete root folder')

        remote_path = self._find_remote_path(c_path)
        if not remote_path.exists():
            return False
        # We have at least one segment ; this is either a folder or a blob
        # (so we cannot rely on deepest_folder_id() as it works only for folders)
        self._delete_by_id(c_path, remote_path.files_chain[-1]['id'])
        return True

    def get_file(self, c_path):
        """Get CFile for given path, or None if no object exists with that path"""
        if c_path.is_root():
            return CFolder(CPath('/'))
        remote_path = self._find_remote_path(c_path, detailed=True)
        if not remote_path.exists():
            return None
        #print("last file = ",remote_path.files_chain[-1])
        return self._parse_c_file_json(c_path.parent(), remote_path.files_chain[-1])

    def download(self, download_request):
        self._retry_strategy.invoke_retry(self._do_download, download_request)

    def _do_download(self, download_request):
        """This method does NOT retry request"""
        c_path = download_request.path
        remote_path = self._find_remote_path(c_path, detailed=True)
        if not remote_path.exists():
            raise CFileNotFoundError('File not found: %r' % c_path, c_path)
        if remote_path.exists() and not remote_path.last_is_blob():
            # path refer to an existing folder: wrong !
            raise CInvalidFileTypeError(c_path, True)

        blob = remote_path.get_blob()
        try:
            url = blob['downloadUrl']
        except KeyError:
            # a blob without a download url is likely a google doc, not downloadable:
            if 'mimeType' in blob and blob['mimeType'].startswith('application/vnd.google-apps.'):
                raise CInvalidFileTypeError(c_path, True, 'google docs are not downloadable: %r' % (c_path,))
            raise CStorageError('No downloadUrl defined for blob: %r' % (c_path,))
        ri = self._get_basic_request_invoker(c_path)
        headers = download_request.get_http_headers()
        with contextlib.closing(ri.get(url,
                                       headers=headers,
                                       stream=True)) as response:
            download_data_to_sink(response, download_request.byte_sink())

    def upload(self, upload_request):
        return self._retry_strategy.invoke_retry(self._do_upload, upload_request)

    def _do_upload(self, upload_request):
        # Check before upload: is it a folder ?
        # (uploading a blob would create another file with the same name: bad)
        c_path = upload_request.path
        remote_path = self._find_remote_path(c_path)
        if remote_path.exists() and not remote_path.last_is_blob():
            # path refer to an existing folder: wrong !
            raise CInvalidFileTypeError(c_path, True)
        if not remote_path.exists() and remote_path.last_is_blob():
            # some blob exists in path: wrong !
            raise CInvalidFileTypeError(remote_path.last_c_path(), False)

        # only one of these 2 will be set:
        file_id = None
        parent_id = None
        if remote_path.exists():
            # Blob already exists: we'll update it
            file_id = remote_path.get_blob()['id']
        else:
            parent_id = remote_path.deepest_folder_id()
            # We may need to create intermediary folders first:
            # create intermediary folders, in case:
            i = len(remote_path.files_chain)
            while i < len(remote_path.segments)-1:
                current_c_path = remote_path.first_segments_path(i+1)
                parent_id = self._raw_create_folder(current_c_path, parent_id)
                i += 1

        # By now we can upload a new blob to folder with id=parent_id,
        # or update existing blob with id=file_id:

        # TODO handle metadata:
        # if upload_request._medatada:

        ri = self._get_api_request_invoker(c_path)
        if file_id:
            # Blob update
            json_meta = {}
        else:
            # Blob creation
            json_meta = {'title': c_path.base_name(), 'parents': [{'id': parent_id}]}
        if upload_request._content_type:
            # It seems that drive distinguishes between mimeType defined here,
            # and Content-Type defined in part header.
            # Drive also tries to guess mimeType...
            json_meta['mimeType'] = upload_request._content_type  # seems to be ignored ? mimeType not updatable ?
        in_stream = upload_request.byte_source().open_stream()
        fields = (RequestField(name=None,
                               data=json.dumps(json_meta),
                               filename=None,
                               headers={'Content-Type': 'application/json; charset=UTF-8'}),
                  RequestField(name=None,
                               data=in_stream,
                               filename=None,
                               headers={'Content-Type': upload_request._content_type}))
        encoder = MultipartEncoder(fields, 'related')
        try:
            if file_id:
                # Updating existing file:
                resp = ri.put(self.FILES_UPLOAD_ENDPOINT + '/' + file_id,
                              headers={'Content-Type': encoder.content_type},  # multipart/related
                              params={'uploadType': 'multipart'},
                              data=encoder)
            else:
                # uploading a new file:
                resp = ri.post(self.FILES_UPLOAD_ENDPOINT,
                               headers={'Content-Type': encoder.content_type},  # multipart/related
                               params={'uploadType': 'multipart'},
                               data=encoder)
        finally:
            in_stream.close()


class GoogleDriveRequestInvoker(RequestInvoker):
    """This special class is for refreshing access_token once if we get
    an authentication error (sometimes valid access tokens are rejected by google), for example see:
    http://stackoverflow.com/questions/14510947/google-drive-api-random-401-errors"""
    def __init__(self, session_manager, validate_func, c_path):
        super(GoogleDriveRequestInvoker, self).__init__(c_path)
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
                raise CRetriableError(ae, delay=0)
            raise ae
        return response


def _parse_date_time(dt_str):
    return dateutil.parser.parse(dt_str)
