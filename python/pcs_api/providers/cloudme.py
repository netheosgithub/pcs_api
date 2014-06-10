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
import xml.etree.ElementTree as ET
import xml.sax.saxutils
from requests.packages.urllib3.fields import RequestField

from ..storage import IStorageProvider, register_provider
from ..oauth.session_managers import DigestAuthSessionManager
from ..models import (CPath, CBlob, CFolder, CQuota,
                      CUploadRequest, CDownloadRequest, RetryStrategy, RequestInvoker)
from ..cexceptions import CRetriableError, CAuthenticationError, CInvalidFileTypeError, CFileNotFoundError, CStorageError
from ..utils import (abbreviate, buildCStorageError, ensure_content_type_is_xml, download_data_to_sink, MultipartEncoder)


logger = logging.getLogger(__name__)

@register_provider
class CloudMeStorage(IStorageProvider):
    """FIXME work in progress !
    CloudMe accepts a blob and a folder with the same names: in this case we always consider folder first.
    Double quotes are forbidden in blob names.
    Content-Type seems to be derived from blob basename: foo.jpg -> image/jpeg, foo.txt -> test/plain and so on.
    """

    PROVIDER_NAME = 'cloudme'

    # Cloudme endpoint:
    ENDPOINT = 'https://www.cloudme.com/v1'
    SOAP_HEADER = ("<SOAP-ENV:Envelope xmlns:" +
                   "SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" " +
                   "SOAP-ENV:encodingStyle=\"\" " +
                   "xmlns:xsi=\"http://www.w3.org/1999/XMLSchema-instance\" " +
                   "xmlns:xsd=\"http://www.w3.org/1999/XMLSchema\">" +
                   "<SOAP-ENV:Body>")
    SOAP_FOOTER = "</SOAP-ENV:Body></SOAP-ENV:Envelope>"

    def __init__(self, storage_builder):
        self._session_manager = DigestAuthSessionManager(storage_builder.user_credentials)
        self._retry_strategy = storage_builder.retry_strategy
        self._root_id = None  # lazily retrieved

    def _buildCStorageError(self, response, c_path):
        """Try to extract error message from xml body:
        soap errors generates http 500 Internal server errors, and body looks like:
        <?xml version='1.0' encoding='utf-8'?>
        <SOAP-ENV:Envelope xmlns:SOAP-ENV='http://schemas.xmlsoap.org/soap/envelope/' SOAP-ENV:encodingStyle='http://schemas.xmlsoap.org/soap/encoding/'>
        <SOAP-ENV:Body>
        <SOAP-ENV:Fault>
        <faultcode>SOAP-ENV:Client</faultcode>
        <faultstring>Not Found</faultstring>
        <detail>
            <error number='0' code='404' description='Not Found'>Document not found.</error>
        </detail>
        </SOAP-ENV:Fault>
        </SOAP-ENV:Body>
        </SOAP-ENV:Envelope>"""
        message = None
        retriable = False
        try:
            ct = response.headers['Content-Type']
            if 'text/xml' in ct or 'application/xml' in ct:
                dom_root = ET.fromstring(response.content)
                faultcode = dom_root.find('.//faultcode')
                #print("ERROR handling: faultcode=",faultcode)
                if faultcode is not None and faultcode.text == 'SOAP-ENV:Client':
                    faultstring = dom_root.find('.//faultstring')
                    if faultstring is not None:
                        # In case we have no detail:
                        message = faultstring.text
                    detail_error = dom_root.find('.//error')
                    #print("ERROR handling: detail_error=", detail_error)
                    if detail_error is not None and 'code' in detail_error.attrib:
                        code = detail_error.attrib['code']
                        reason = detail_error.attrib['description']
                        number = detail_error.attrib['number']
                        message = '[%s %s (%s)]' % (code, reason, number)
                        message += ' ' + detail_error.text
                        if c_path:
                            message += ' (%r)' % c_path
                        if code == '404':
                            #print("Building CFilenotFoundError with msg=",message,"  and c_path=",c_path)
                            return CFileNotFoundError(message, c_path)
                        # These errors are not retriable, as we have received a well formed response
            else:
                # We haven't received a standard server error message ?!
                # This can happen unlikely. Usually such errors are temporary.
                message = response.text  # This will be the exception error message
                logger.error('Unparsable server error: %s', message)
                logger.error('Unparsable server error has headers: %s', response.headers)
                message = abbreviate('Unparsable server error: ' + message, 200)
                if response.status_code >= 500:
                    retriable = True
        except:
            pass
        err = buildCStorageError(response, message, c_path)
        if retriable:
            err = CRetriableError(err)
        return err

    def _validate_cloudme_api_response(self, response, c_path):
        """Validate a response from CloudMe XML API.

        An API response is valid if response is valid, and content-type is xml."""

        self._validate_cloudme_response(response, c_path)
        # Server response looks correct ; however check content type is XML:
        ensure_content_type_is_xml(response, raise_retriable=True)
        # OK, response looks fine:
        return response

    def _validate_cloudme_response(self, response, c_path):
        """Validate a response for a file download or API request.

        Only server code is checked (content-type is ignored)."""
        logger.debug("validating cloudme response: %s %s: %d %s",
                     response.request.method,
                     response.request.url,
                     response.status_code, response.reason)

        if response.status_code >= 300:
            # Determining if error is retriable is not possible without parsing response:
            raise self._buildCStorageError(response, c_path)
        # OK, response looks fine:
        return response

    def _get_request_invoker(self, validation_function, c_path):
        request_invoker = RequestInvoker(c_path)
        session = self._session_manager.get_session()
        # We forward directly to session request method:
        request_invoker.do_request = session.request
        request_invoker.validate_response = validation_function
        return request_invoker

    def _get_basic_request_invoker(self, c_path=None):
        """An invoker that does not check response content type:
        to be used for files downloading"""
        return self._get_request_invoker(self._validate_cloudme_response, c_path)

    def _get_api_request_invoker(self, c_path=None):
        """An invoker that checks response content type = json:
        to be used by all API requests"""
        return self._get_request_invoker(self._validate_cloudme_api_response, c_path)

    def _build_rest_url(self, method_path):
        return self.ENDPOINT + '/' + method_path + '/'

    def _do_soap_request(self, action, inner_xml, c_path=None):
        """Do a soap request, return server response as dom object (Element Tree)"""
        ri = self._get_api_request_invoker(c_path)
        soap = list()
        soap.append(self.SOAP_HEADER);
        soap.append('<' + action + '>');
        soap.append(inner_xml);
        soap.append('</' + action + '>');
        soap.append(self.SOAP_FOOTER);
        data = ''.join(soap).encode('UTF-8')
        resp = self._retry_strategy.invoke_retry(ri.post,
                                                 url=self.ENDPOINT,
                                                 data=data,
                                                 headers={"soapaction": action,
                                                          "Content-Type": "text/xml; charset=utf-8"})
        # parse xml response to DOM:
        #print(resp.content)
        dom_root = ET.fromstring(resp.content)  # ET needs bytes in python2
        return dom_root

    def provider_name(self):
        return CloudMeStorage.PROVIDER_NAME

    def _get_login(self):
        return self._do_soap_request(action='login', inner_xml='')

    def _list_blobs(self, parent_cm_folder, parent_c_path):
        """List all blobs present in given folder"""
        dom_root = self._do_soap_request('queryFolder',
                                         '<folder id="%s"/>' % parent_cm_folder.file_id,
                                         parent_c_path)
        elements = dom_root.findall('.//{http://www.w3.org/2005/Atom}entry')
        cm_blobs = list()
        for element in elements:
            cm_blobs.append(CMBlob.build_CMBlob_from_xml( parent_cm_folder, element))
        return cm_blobs

    def _get_blob_by_name(self, cm_parent_folder, base_name):
        # We use double quotes around base_name, as double quotes are never used in blob names:
        inner_xml = "<folder id='%s'/>" \
                    '<query>"%s"</query>' \
                    "<count>1</count>" % (cm_parent_folder.file_id, escape_xml_string(base_name))
        dom_root = self._do_soap_request('queryFolder', inner_xml)
        entry = dom_root.find('.//{http://www.w3.org/2005/Atom}entry' )
        if entry is None:
            return None

        cm_blob = CMBlob.build_CMBlob_from_xml(cm_parent_folder, entry)
        return cm_blob

    def _load_folders_structure(self):
        """Gets the tree structure beginning from a given folder"""
        root_id = self._get_root_id()
        #print("found root id = ",root_id)
        dom_root = self._do_soap_request('getFolderXML', '<folder id="%s"/>' % root_id)
        root_folder_element = self._find_root_element(dom_root, root_id)
        #print("found root folder element = ",root_folder_element)
        root_folder = CMFolder(root_id, 'root')
        self._scan_folder_level(root_folder_element, root_folder)
        return root_folder

    def _find_root_element(self, dom_root, root_folder_id):
        """Find the element corresponding to the root folder in the DOM (raise if not found)"""
        #print("_find_root_element(): dom_root=",dom_root,dom_root.tag)
        folders = dom_root.findall('./{http://schemas.xmlsoap.org/soap/envelope/}Body'
                                   '/{http://xcerion.com/xcRepository.xsd}getFolderXMLResponse'
                                   '/{http://xcerion.com/folders.xsd}folder')
        root_id = self._get_root_id()
        for element in folders:
            #print("_find_root_element: element=",element,"  id=",element.attrib['id'])
            if element.attrib['id'] == root_id:
                return element
        raise ValueError("Not found root folder with id="+root_folder_id)

    def _scan_folder_level(self, element, cm_folder):
        #print("scan_folder_element: element=",element, "  folder=",cm_folder)
        for current_element in element.findall('{http://xcerion.com/folders.xsd}folder'):
            #print("scanning current element=",current_element, current_element.attrib['id'])
            # calls this method for all the children which is Element
            child_folder = cm_folder.add_child(current_element.attrib['id'],
                                               current_element.attrib['name'])
            self._scan_folder_level(current_element, child_folder)

    def _get_root_id(self):
        # We do not lock here: it may happen at start that several threads will issue same request
        if not self._root_id:
            self._root_id = self._get_login().find(".//home").text
        return self._root_id

    def get_user_id(self):
        """user_id is login in case of CloudMe"""
        dom_root = self._get_login()
        return dom_root.find('.//username').text

    def get_quota(self):
        """Return a CQuota object.

        Shared files are not counted in used bytes: ."""
        dom_root = self._get_login()
        a_drive = dom_root.find('.//drives/drive')
        return CQuota(int(a_drive.find('currentSize').text), int(a_drive.find('quotaLimit').text))

    def list_root_folder(self):
        return self.list_folder(CPath('/'))

    def list_folder(self, c_folder_or_c_path):
        """There are 3 main steps to list a folder:
        - generate the tree view of CloudMe storage
        - list all the blobs
        - list all the subfolders
        """
        try:
            c_path = c_folder_or_c_path.path
        except AttributeError:
            c_path = c_folder_or_c_path

        cm_root = self._load_folders_structure()
        cm_folder = cm_root.get_folder(c_path)
        if not cm_folder:
            # Folder does not exist: we must check if it is a blob
            cm_folder = cm_root.get_folder(c_path.parent())
            if not cm_folder:
                # parent folder does not exist either so we are sure:
                return None
            cm_blob = self._get_blob_by_name(cm_folder, c_path.base_name())
            if cm_blob:
                raise CInvalidFileTypeError(c_path, False)
            # Nothing exists at that path
            return None

        c_folder_content = dict()
        # Adding folders:
        for child_folder in cm_folder:
            c_folder = child_folder.to_CFolder()
            c_folder_content[c_folder.path] = c_folder
        # Adding blobs:
        cm_blobs = self._list_blobs(cm_folder, c_path)
        for cm_blob in cm_blobs:
            c_blob = cm_blob.to_CBlob()
            c_folder_content[c_blob.path] = c_blob

        return c_folder_content

    def create_folder(self, c_path):
        if c_path.is_root():
            return False

        cm_root = self._load_folders_structure()
        cm_folder = cm_root.get_folder(c_path)
        if cm_folder:
            # folder already exists
            return False
        self._create_intermediary_folders(cm_root, c_path)
        return True

    def _create_intermediary_folders(self, cm_root_folder, c_path):
        """Creates all intermediary folders given the whole folders structure"""
        segments = c_path.split()
        current_folder = cm_root_folder
        current_c_path = CPath('/')
        first_folder_creation = True
        for segment in segments:
            current_c_path = current_c_path.add(segment)
            child_folder = current_folder.get_child_by_name(segment)
            if not child_folder:
                # Intermediary folder does not exist: has to be created
                if first_folder_creation:
                    # This is the first intermediary folder to create:
                    # let's check that there is no blob with that name already existing
                    cm_blob = self._get_blob_by_name(current_folder, segment)
                    if cm_blob:
                        raise CInvalidFileTypeError(cm_blob.get_c_path(), False)
                child_folder = self._raw_create_folder(current_folder, current_c_path, segment)
                first_folder_creation = False
            current_folder = child_folder
        return child_folder

    def _raw_create_folder(self, parent_folder, parent_c_path, name):
        inner_xml = '<folder id="%s"/><childFolder>%s</childFolder>' % (parent_folder.file_id,
                                                                        escape_xml_string(name))
        dom_root = self._do_soap_request('newFolder', inner_xml, parent_c_path)
        new_folder_id = dom_root.find('.//newFolderId').text
        return CMFolder(new_folder_id, name)

    def _delete_by_id(self, c_path, file_id):
        url = self._get_file_url(file_id) + '/trash'
        ri = self._get_api_request_invoker(c_path)
        resp = self._retry_strategy.invoke_retry(ri.post, url)

    def delete(self, c_path):
        if c_path.is_root():
            raise CStorageError('Can not delete root folder')
        cm_root = self._load_folders_structure()
        cm_parent_folder = cm_root.get_folder(c_path.parent())
        if not cm_parent_folder:
            # parent folder of given path does exist = > path does not exist
            return False

        cm_folder = cm_parent_folder.get_child_by_name(c_path.base_name())
        if cm_folder:
            # We have to delete a folder:
            inner_xml = '<folder id="%s"/><childFolder id="%s"/>' % (cm_parent_folder.file_id, cm_folder.file_id)
            dom_root = self._do_soap_request('deleteFolder', inner_xml, c_path)
            result = dom_root.find('.//{http://xcerion.com/xcRepository.xsd}deleteFolderResponse').text
            return result.strip().upper() == "OK"

        # It's not a folder, it may be a blob... We'll request by name and check response:
        inner_xml = '<folder id="%s"/><document>%s</document>' % (cm_parent_folder.file_id,
                                                                  escape_xml_string(c_path.base_name()))
        try:
            dom_root = self._do_soap_request('deleteDocument', inner_xml, c_path)
            result = dom_root.find('.//{http://xcerion.com/xcRepository.xsd}deleteDocumentResponse').text
            return result.strip().upper() == "OK"
        except CFileNotFoundError as e:
            print("catch CFilenotFoundError: message=",e.message, '  c_path', e.path)
            # No blob has been found with this name:
            return False

    def get_file(self, c_path):
        """Get CFile for given path, or None if no object exists with that path"""
        if c_path.is_root():
            return CFolder(CPath('/'))
        cm_root = self._load_folders_structure()
        parent_c_path = c_path.parent()
        cm_parent_folder = cm_root.get_folder(parent_c_path)
        if not cm_parent_folder:
            # parent folder of given path does exist = > path does not exist
            return None
        cm_folder = cm_parent_folder.get_child_by_name(c_path.base_name())
        if cm_folder:
            # the path corresponds to a folder
            return cm_folder.to_CFolder()

        # It's not a folder, it should be a blob...
        cm_blob = self._get_blob_by_name(cm_parent_folder, c_path.base_name())
        if not cm_blob:
            # Nothing found:
            return None
        return cm_blob.to_CBlob()

    def download(self, download_request):
        self._retry_strategy.invoke_retry(self._do_download, download_request)

    def _do_download(self, download_request):
        """This method does NOT retry request"""
        c_path = download_request.path

        cm_root = self._load_folders_structure()
        parent_c_path = c_path.parent()
        cm_parent_folder = cm_root.get_folder(parent_c_path)
        if not cm_parent_folder:
            # parent folder of given path does exist => file does not exist
            raise CFileNotFoundError( "This file does not exist", c_path )

        cm_folder = cm_parent_folder.get_child_by_name(c_path.base_name())
        if cm_folder:
            # the path corresponds to a folder
            raise CInvalidFileTypeError(c_path, True )

        # It's not a folder, it should be a blob...
        cm_blob = self._get_blob_by_name(cm_parent_folder, c_path.base_name())
        if not cm_blob:
            # File does not exist
            raise CFileNotFoundError( "This file does not exist", c_path)
        url = self._build_rest_url('documents') + cm_parent_folder.file_id + '/' + cm_blob.file_id + "/1"
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
        cm_root = self._load_folders_structure()
        cm_parent_folder = cm_root.get_folder(parent_c_path)
        if not cm_parent_folder:
            # parent folder of given path does exist => folders needs to be created
            cm_parent_folder = self._create_intermediary_folders(cm_root, parent_c_path)

        # Check: is it an existing folder ?
        cm_folder = cm_parent_folder.get_child_by_name(base_name)
        if cm_folder:
            # The CPath corresponds to an existing folder, upload is not possible
            raise CInvalidFileTypeError(c_path, True)

        url = self._build_rest_url('documents') + cm_parent_folder.file_id
        in_stream = upload_request.byte_source().open_stream()
        rf_bin = RequestField(name='bin',
                              data=in_stream,
                              filename=c_path.base_name())
        # Cloudme does not support UTF-8 encoded filenames that look like:
        # filename*=UTF-8''...
        # Instead we send raw UTF-8 bytes in between quotes (filename cannot contain any quote)
        rf_bin.headers['Content-Disposition'] = 'form-data; name="bin"; filename="%s"' % base_name
        rf_bin.headers['Content-Type'] = upload_request._content_type  # actually ignored...
        encoder = MultipartEncoder((rf_bin,))
        try:
            ri = self._get_api_request_invoker(c_path)
            resp = ri.post(url=url,
                           headers={'Content-Type': encoder.content_type},  # multipart/form-data
                           data=encoder)
        finally:
            in_stream.close()


def _parse_date_time(dt_str):
    return dateutil.parser.parse(dt_str)


class CMFolder(object):

    def __init__(self, file_id, name, parent_cmfolder=None):
        self.file_id = file_id
        self.name = name
        self.parent = parent_cmfolder
        self.children = {}  # to be populated: key=name, value=CMFolder

    def add_child(self, child_id, child_name):
        child = CMFolder(child_id, child_name, self)
        self.children[child_name] = child
        return child

    def __iter__(self):
        return self.children.itervalues()

    def get_child_by_name(self, name):
        try:
            return self.children[name]
        except KeyError:
            return None

    def get_folder(self, c_path):
        """Gets the CMFolder corresponding to a given CPath (only works if self is root CMFolder).
          :returns None if the folder does not exist
        """
        segments = c_path.split()
        current_folder = self  # must be root folder !
        for segment in segments:
            sub_folder = current_folder.get_child_by_name(segment)
            if not sub_folder:
                return None
            current_folder = sub_folder
        return current_folder

    def get_c_path(self):
        if not self.parent:
            return CPath('/')

        current_folder = self
        path = ''
        while current_folder.parent:
            path = '/' + current_folder.name + path
            current_folder = current_folder.parent
        return CPath(path)

    def to_CFolder(self):
        return CFolder(path=self.get_c_path(), file_id=self.file_id)


class CMBlob(object):

    @staticmethod
    def build_CMBlob_from_xml(parent_cm_folder, xml_entry):
        name = xml_entry.find('{http://www.w3.org/2005/Atom}title').text
        file_id = xml_entry.find('{http://xcerion.com/directory.xsd}document').text

        modification_time_str = xml_entry.find('{http://www.w3.org/2005/Atom}updated').text
        # looks like '2014-03-26T15:28:07Z'
        modification_time = dateutil.parser.parse(modification_time_str)
        link = xml_entry.find('{http://www.w3.org/2005/Atom}link')
        content_type = link.attrib['type']
        length = int(link.attrib['length'])
        return CMBlob( parent_cm_folder, file_id, name, length, modification_time, content_type)

    def __init__(self, cm_folder, file_id, name, length, modification_time, content_type):
        self.folder = cm_folder
        self.file_id = file_id
        self.name = name
        self.length = length
        self.modification_time = modification_time
        self.content_type = content_type

    def to_CBlob(self):
        return CBlob( self.length, self.content_type,
                      self.get_c_path(), file_id=self.file_id, modification_time=self.modification_time )

    def get_c_path(self):
        """Gets CPath of this CloudMe Blob, which is its parent folder's CPath + its name"""
        return self.folder.get_c_path().add(self.name)


def escape_xml_string(s):
    """Escape a string for putting into xml fragment"""
    return xml.sax.saxutils.escape(s)


