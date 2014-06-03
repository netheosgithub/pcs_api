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
import contextlib
import datetime
import requests
import logging
import email.header
import dateutil.parser
import dateutil.tz

from ..models import (CPath, CBlob, CFolder, CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import (CRetriableError, CInvalidFileTypeError, CFileNotFoundError, CStorageError)
from ..utils import (buildCStorageError, ensure_content_type_is_json, shorten_url,
                     download_data_to_sink, get_content_length, get_content_type)

logger = logging.getLogger(__name__)


class SwiftClient(object):
    """Openstack swift storage client. This is _not_ a IStorageProvider object.

    Note: from user point of view, account and container do not appear in objects path.
    Account is part of endpoint URL.
    Container must be defined by user with method use_container(), or use_first_container() must be called."""

    CONTENT_TYPE_DIRECTORY = 'application/directory'

    def __init__(self, account_endpoint, x_auth_token,
                 retry_strategy,
                 with_directory_markers=True):
        self._account_endpoint = account_endpoint
        self.current_container = None
        self._x_auth_token = x_auth_token
        self._with_directory_markers = with_directory_markers
        self._retry_strategy = retry_strategy

    def _buildCStorageError(self, response, message, c_path):
        # Swift error messages are only in reason, so nothing to extract here :
        return buildCStorageError(response, message, c_path)

    def _validate_swift_response(self, response, c_path):
        """A response is valid if server code is 2xx.
        It is recoverable in case of server error 5xx and some 4xx errors"""
        logger.debug("validating swift response: %s %s : %d %s",
                     response.request.method,
                     response.request.url,
                     response.status_code, response.reason)
        code = response.status_code
        if code < 300:
            return response
        if (code >= 500 or
                code == 498 or code == 429 or  # too many requests
                code == 408):  # request timeout
            # We force connection closing in that case,
            # to avoid being sticked on a non-healthy server :
            response.connection.close()
            raise CRetriableError(self._buildCStorageError(response, message=None, c_path=c_path))
        # Other cases :
        raise self._buildCStorageError(response, message=None, c_path=c_path)

    def _validate_swift_api_response(self, response, c_path):
        """Validate swift response, and also checks content is empty, or content-type is json."""
        self._validate_swift_response(response, c_path)
        cl = get_content_length(response)
        if cl is not None and cl > 0:
            ensure_content_type_is_json(response)
        return response

    def _get_session(self, response_format=None):
        session = requests.Session()
        session.headers.update({'X-Auth-token': self._x_auth_token})
        if response_format:
            session.params = {'format': response_format}
        return session

    def _get_request_invoker(self, session, validation_function, c_path):
        request_invoker = RequestInvoker(c_path)
        # We forward directly to request() method :
        request_invoker.do_request = session.request
        request_invoker.validate_response = validation_function
        return request_invoker

    def _get_basic_request_invoker(self, c_path=None):
        """Suitable for files downloads requests"""
        session = self._get_session()
        return self._get_request_invoker(session, self._validate_swift_response, c_path)

    def _get_api_request_invoker(self, c_path=None):
        """Suitable for API requests : ask for json response, and check content-type"""
        session = self._get_session(response_format='json')
        return self._get_request_invoker(session, self._validate_swift_api_response, c_path)

    def _head_or_none(self, c_path):
        """Perform a quick HEAD request on the given object,
        to check existence and type.

        returns headers, or None if no object exists at this path"""
        ri = self._get_basic_request_invoker(c_path)
        url = self.get_object_url(c_path)
        try:
            return self._retry_strategy.invoke_retry(ri.head, url).headers
        except CFileNotFoundError as e:
            return None

    def get_containers(self):
        ri = self._get_api_request_invoker()
        containers = self._retry_strategy.invoke_retry(ri.get, self._account_endpoint).json()
        logger.debug('Available containers: %s', containers)
        return containers

    def use_container(self, container):
        self.current_container = container
        logger.debug('Using container: %s', container)

    def use_first_container(self):
        containers = self.get_containers()
        if len(containers) == 0:
            raise ValueError("Account %s has no container ?!" % self._account_endpoint)
        self.use_container(containers[0]['name'])
        if len(containers) > 1:
            logger.warning('Account %s has %d containers: choosing first one as current: %s',
                            self._account_endpoint, len(containers), self.current_container)
        return self.current_container

    def _ensure_current_container_is_set(self):
        if self.current_container is None:
            raise ValueError("Undefined current container for account %s" % self._account_endpoint)

    def get_current_container_url(self):
        self._ensure_current_container_is_set()
        return "%s/%s" % (self._account_endpoint, self.current_container)

    def get_object_url(self, object_c_path):
        """Url encode object path, and concatenate to current container URL to get full URL"""
        container_url = self.get_current_container_url()
        return "%s%s" % (container_url, object_c_path.url_encoded())

    def _raw_create_folder(self, folder_c_path):
        """Create a folder without creating any higher level intermediary folders."""
        url = self.get_object_url(folder_c_path)
        ri = self._get_api_request_invoker(folder_c_path)
        self._retry_strategy.invoke_retry(ri.put,
                                          url=url,
                                          headers={'Content-Type': SwiftClient.CONTENT_TYPE_DIRECTORY})

    def create_intermediary_folders_objects(self, leaf_folder_path):
        """Create any parent folders if they do not exist, to meet old swift convention.

        hubiC requires these objects for the sub-objects to be visible in webapp.
        As an optimization, we consider that if folder a/b/c exists, then a/ and a/b/ also exist
        so are not checked nor created."""
        # We check for folder existence before creation,
        # as in general leaf folder is likely to already exist.
        # So we walk from leaf to root:
        c_path = leaf_folder_path
        parent_folders = []
        while not c_path.is_root():  # deepest first
            c_file = self.get_file(c_path)
            if c_file is not None:
                if c_file.is_blob():
                    # Problem here: clash between folder and blob
                    raise CInvalidFileTypeError(c_file.path, False)
                break
            else:
                logger.debug("Nothing exists at path: %s, will go up", c_path)
                parent_folders.insert(0, c_path)
                c_path = c_path.parent()
        # By now we know which folders to create:
        if parent_folders:
            logger.debug("Inexisting parent_folders will be created : %r", parent_folders)
            for c_path in parent_folders:
                logger.debug("Creating intermediary folder: %r", c_path)
                self._raw_create_folder(c_path)


    def get_file(self, c_path):
        """Inquire details about object at given path.

        Return: a CFolder, CBlob or None if no object exist at this path"""
        headers = self._head_or_none(c_path)
        if headers is None:
            return None
        if not 'Content-Type' in headers:
            logger.warning("%s object has no content type ?!" % (c_path,))
            return None
        if headers['Content-Type'] != SwiftClient.CONTENT_TYPE_DIRECTORY:
            c_file = CBlob(int(headers['Content-Length']),
                           headers['Content-Type'],
                           c_path,
                           modification_time=parse_x_timestamp(headers),
                           metadata=parse_x_meta_headers(headers))
        else:
            c_file = CFolder(c_path,
                             modification_time=parse_x_timestamp(headers),
                             metadata=parse_x_meta_headers(headers))
        return c_file

    def _list_objects_within_folder(self, c_path, delimiter=None):
        ri = self._get_api_request_invoker(c_path)
        url = self.get_current_container_url()
        # prefix should not start with a slash, but end with a slash:
        # '/path/to/folder' --> 'path/to/folder/'
        prefix = c_path.path_name()[1:] + '/'
        if prefix == '/':
            prefix = ''
        json = self._retry_strategy.invoke_retry(ri.get,
                                                 url=url,
                                                 params={'delimiter': delimiter, 'prefix': prefix}).json()
        return json

    def list_folder(self, c_folder_or_c_path):
        """Return map of CFile in given folder. Key is file CPath

        :param c_folder_or_c_path: the CFolder object or CPath to be listed"""
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path
        json = self._list_objects_within_folder(c_path, delimiter='/')
        if not json:
            # List is empty ; can be caused by a really empty folder,
            # a non existing folder, or a blob
            # Distinguish the different cases :
            c_file = self.get_file(c_path)
            if c_file is None:  # Nothing at that path
                return None
            if c_file.is_blob():  # It is a blob : error !
                raise CInvalidFileTypeError(c_path, False)
        ret = {}
        for obj in json:
            if 'subdir' in obj:  # indicates a non empty sub directory
                # There are two cases here : provider uses directory-markers, or not.
                # - if yes, another entry should exist in json with more detailed informations.
                # - if not, this will be the only entry that indicates a sub folder,
                #   so we keep this file, but we'll memorize it only if it is not already present
                #   in returned value.
                c_file = CFolder(CPath(obj['subdir']))
                detailed = False
            else:
                detailed = True
                if obj['content_type'] != SwiftClient.CONTENT_TYPE_DIRECTORY:
                    c_file = CBlob(obj['bytes'],
                                   obj['content_type'],
                                   CPath(obj['name']),
                                   modification_time=parse_last_modified(obj),
                                   metadata=None)  # we do not have this detailed information
                else:
                    c_file = CFolder(CPath(obj['name']),
                                     modification_time=parse_last_modified(obj),
                                     metadata=None)  # we do not have this detailed information
            if detailed or not c_file.path in ret:
                # If we got a detailed file, we always store it
                # If we got only rough description, we keep it only if no detailed info already exists
                ret[c_file.path] = c_file
        return ret

    def create_folder(self, c_path):
        c_file = self.get_file(c_path)
        if c_file:
            if c_file.is_folder():
                return False  # folder already exists
            # It is a blob : error !
            raise CInvalidFileTypeError(c_path, False)
        if self._with_directory_markers:
            self.create_intermediary_folders_objects(c_path.parent())
        self._raw_create_folder(c_path)
        return True

    def delete(self, c_path):
        """If path references a folder, this is a lengthly operation because
        all sub-objects must be deleted one by one.
        (as of 2014-02, hubic swift does not seem to support bulk deletes :
        http://docs.openstack.org/developer/swift/middleware.html#module-swift.common.middleware.bulk )
        """
        if c_path.is_root():
            raise CStorageError('Can not delete root folder')

        # Request sub-objects w/o delimiter : all sub-objects are returned
        # In case c_path is a blob, we'll get an empty list
        json = self._list_objects_within_folder(c_path)
        # Now delete all objects ; we start with the deepest ones so that
        # in case we are interrupted, directory markers are still present
        # Note : swift may guarantee that list is ordered, but could not confirm information...
        pathnames = sorted(map(lambda f: '/'+f['name'], json), reverse=True)
        # Now we also add that top-level folder (or blob) to delete :
        pathnames.append(c_path.path_name())
        at_least_one_delete = False
        for pathname in pathnames:
            last_delete_worked = False
            logging.debug("deleting object at path : %s", pathname)
            current_path = CPath(pathname)
            url = self.get_object_url(current_path)
            try:
                ri = self._get_api_request_invoker(current_path)
                self._retry_strategy.invoke_retry(ri.delete, url=url)
                at_least_one_delete = True
            except CFileNotFoundError:
                pass
        return at_least_one_delete

    def download(self, download_request):
        self._retry_strategy.invoke_retry( self._do_download, download_request)

    def _do_download(self, download_request):
        url = self.get_object_url(download_request.path)
        headers = download_request.get_http_headers()
        ri = self._get_basic_request_invoker(download_request.path)
        with contextlib.closing(ri.get(url=url,
                                       headers=headers,
                                       stream=True)) as response:
            # It is an error to download a folder :
            if get_content_type(response) == SwiftClient.CONTENT_TYPE_DIRECTORY:
                raise CInvalidFileTypeError(download_request.path, True)
            download_data_to_sink(response, download_request.byte_sink())

    def upload(self, upload_request):
        self._retry_strategy.invoke_retry( self._do_upload, upload_request)

    def _do_upload(self, upload_request):
        # Check before upload : is it a folder ?
        # (uploading a blob to a folder would work, but owuld hide all folder sub-files)
        c_file = self.get_file(upload_request.path)
        if c_file and c_file.is_folder():
            raise CInvalidFileTypeError(c_file.path, True)
        if self._with_directory_markers:
            self.create_intermediary_folders_objects(upload_request.path.parent())
        url = self.get_object_url(upload_request.path)
        headers = {}
        if upload_request._content_type:
            headers['Content-Type'] = upload_request._content_type
        if upload_request._metadata:
            add_metadata_headers(headers, upload_request._metadata)
        ri = self._get_basic_request_invoker(upload_request.path)
        in_stream = upload_request.byte_source().open_stream()
        try:
            # httplib does its own loop for sending ;
            ri.put(url, headers=headers, data=in_stream)
        finally:
            in_stream.close()


def parse_last_modified(json):
    try:
        lm = json['last_modified']
        # Force a timezone in datetime object :
        if not '+' in lm:
            lm += '+0000'
        return dateutil.parser.parse(lm)
    except KeyError:
        logging.warning("No last_modified entry in JSON : %s", json)
        return None


def parse_x_timestamp(headers):
    header_value = None
    try:
        header_value = headers['X-Timestamp']
        ts = float(header_value)
        dt = datetime.datetime.fromtimestamp(ts, dateutil.tz.tzutc())
        return dt
    except KeyError:
        logging.warning('No X-Timestamp header found ?!')
        return None
    except ValueError:
        logging.warning('Could not convert timestamp value : %s', header_value)
        return None


def parse_x_meta_headers(headers):
    metadata = {}
    for h, v in headers.iteritems():
        try:
            if h.startswith('x-object-meta-'):
                metadata[h[14:]] = unicode(email.header.make_header(email.header.decode_header(v)))
        except UnicodeError as e:
            logger.warning("Could not parse header value : ", v)
    return metadata


def add_metadata_headers(headers, metadata):
    for key, val in metadata.iteritems():
        val = val.replace('\r','').replace('\n','')
        escaped_val = str(email.header.Header(val))
        headers['X-Object-Meta-' + key] = escaped_val


