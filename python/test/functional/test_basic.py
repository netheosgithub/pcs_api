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
import time
import datetime
import dateutil
import os
import pytest
import random

from pcs_api.storage import StorageFacade
from pcs_api.bytes_io import StdoutProgressListener
from pcs_api.models import CPath, CFolder, CBlob, CUploadRequest, CDownloadRequest
from pcs_api.cexceptions import (CStorageError, CInvalidFileTypeError,
                                 CFileNotFoundError, CAuthenticationError, CRetriableError)
from pcs_api.bytes_io import MemoryByteSource, MemoryByteSink, FileByteSink, ProgressByteSink, ProgressByteSource

logger = logging.getLogger(__name__)

# Import tests utility functions & configuration, etc.
import miscutils


def test_registered_providers():
    providers_names = StorageFacade.get_registered_providers()
    logger.info('Registered providers in pcs_api: %s', providers_names)
    assert 'dropbox' in providers_names
    assert 'hubic' in providers_names
    assert 'googledrive' in providers_names


def test_cleanup_storage_before_tests(storage):
    """Not a real test: only some cleanup before next tests"""
    miscutils.cleanup_test_folders(storage)


def test_get_user_id(storage):
    user_id = storage.get_user_id()
    logger.info("Retrieved from provider %s: user_id = %s", storage.provider_name(), user_id)
    assert storage._session_manager._user_credentials.user_id == user_id


def test_display_quota(storage):
    quota = storage.get_quota()
    logger.info("Retrieved quota for provider %s: %r (%.1f%% used)",
                storage.provider_name(), quota, quota.percent_used())


def test_quota_changes_after_upload(storage):
    """Quota is not updated in real time for some providers"""
    if storage.provider_name() == 'hubic' or storage.provider_name() == 'googledrive':
        pytest.xfail("quota not updated in real time for provider %s" % storage.provider_name())

    quota_before = storage.get_quota()
    logger.info("Quota BEFORE upload (%s provider): used=%d / total=%d",
                storage.provider_name(), quota_before.used_bytes, quota_before.allowed_bytes)
    # Upload a quite big blob:
    file_size = 500000
    c_path = miscutils.generate_test_path()
    logger.info('Uploading blob with size %d bytes to %s', file_size, c_path)
    content = bytearray(os.urandom(file_size))
    upload_request = CUploadRequest(c_path, MemoryByteSource(content))
    storage.upload(upload_request)

    logger.info('Checking uploaded file size')
    cblob = storage.get_file(c_path)
    assert isinstance(cblob, CBlob)
    assert cblob.length == file_size

    logger.info('Checking quota has changed')
    quota_after = storage.get_quota()
    storage.delete(c_path)
    logger.info('Quota AFTER upload (%s provider): used=%d / total=%d',
                storage.provider_name(), quota_after.used_bytes, quota_after.allowed_bytes)
    used_difference = quota_after.used_bytes - quota_before.used_bytes
    logger.info('used bytes difference = %d (upload file size was %d)',
                used_difference, file_size)
    assert used_difference == file_size


def test_file_operations(storage):
    # We'll use a temp folder for tests:
    temp_root_path = miscutils.generate_test_path()
    logger.info('Will use test folder: %s', temp_root_path)

    # Create a sub-folder:
    sub_path = temp_root_path.add('sub_folder')
    logger.info('Creating sub_folder: %s', sub_path)
    assert storage.create_folder(sub_path) is True  # True because actually created
    assert storage.create_folder(sub_path) is False  # False because not created
    # Check back:
    sub_folder = storage.get_file(sub_path)
    assert sub_folder.path == sub_path
    assert sub_folder.is_folder()
    assert not sub_folder.is_blob()
    if sub_folder.modification_time:  # Not all providers have a modif time on folders
        miscutils.assert_datetime_is_almost_now(sub_folder.modification_time)

    # Upload 2 files into this sub-folder:
    fpath1 = sub_path.add('a_test_file1')
    content_file1 = b'This is binary cont€nt of test file 1...'
    logger.info('Uploading blob to: %s', fpath1)
    upload_request = CUploadRequest(fpath1, MemoryByteSource(content_file1))
    storage.upload(upload_request)

    fpath2 = sub_path.add('a_test_file2')
    # Generate a quite big random data:
    content_file2 = bytearray(os.urandom(500000))
    logger.info('Uploading blob to: %s', fpath2)
    upload_request = CUploadRequest(fpath2, MemoryByteSource(content_file2))
    storage.upload(upload_request)

    # Check uploaded blobs informations:
    # we check file2 first because has just been uploaded / for modif time check
    cblob = storage.get_file(fpath2)
    assert cblob.is_blob()
    assert not cblob.is_folder()
    assert cblob.length == len(content_file2)
    miscutils.assert_datetime_is_almost_now(cblob.modification_time)

    cblob = storage.get_file(fpath1)
    assert cblob.is_blob()
    assert not cblob.is_folder()
    assert cblob.length == len(content_file1)

    # Download data, and check:
    logger.info('Downloading back and checking file: %s', fpath1)
    mbs = MemoryByteSink()
    download_request = CDownloadRequest(fpath1, mbs)
    storage.download(download_request)
    assert mbs.get_bytes() == content_file1

    logger.info('Downloading back and checking file ranges: %s', fpath1)
    download_request.range(5, None)  # starting at offset 5
    storage.download(download_request)
    assert mbs.get_bytes() == content_file1[5:]
    if storage.provider_name() != 'rapidshare':
        # rapidshare does not support such requests:
        download_request.range(None, 5)  # last 5 bytes
        storage.download(download_request);
        assert mbs.get_bytes() == content_file1[-5:]
    download_request.range(2, 5);  # 5 bytes at offset 2
    storage.download(download_request);
    assert mbs.get_bytes() == content_file1[2:7]

    logger.info('Downloading back and checking file: %s', fpath2)
    download_request = CDownloadRequest(fpath2, mbs)
    storage.download(download_request)
    assert mbs.get_bytes() == content_file2

    # Check that if we upload again, blob is replaced:
    logger.info('Checking file overwrite: %s', fpath2)
    content_file2 = bytearray(os.urandom(300000))  # 300kB file
    upload_request = CUploadRequest(fpath2, MemoryByteSource(content_file2))
    storage.upload(upload_request)
    storage.download(download_request)
    assert mbs.get_bytes() == content_file2

    # Check that we can replace existing blob with empty content:
    if storage.provider_name() != 'googledrive':
        # Google Drive does not support empty content update.
        # See http://stackoverflow.com/questions/12180392/on-google-drive-sdk-how-to-update-a-file-with-an-empty-content-confirmed-bug
        logger.info('Checking file overwrite with empty file: %s', fpath2)
        content_file2 = b''
        upload_request = CUploadRequest(fpath2, MemoryByteSource(content_file2))
        storage.upload(upload_request)
        storage.download(download_request)
        assert mbs.get_bytes() == content_file2

    # Create a sub_sub_folder:
    sub_sub_path = sub_path.add('a_sub_sub_folder')
    logger.info('Creating a_sub_sub_folder: %s', sub_sub_path)
    created = storage.create_folder(sub_sub_path)
    assert created

    logger.info('Check uploaded blobs and sub_sub_folder all appear in folder list')
    folder_content = storage.list_folder(sub_folder)
    logger.info('sub_folder contains files: %r', folder_content)
    # It happened once here that hubic did not list fpath1 'a_test_file1' in folder_content:
    # only 2 files were present ?!
    assert len(folder_content) == 3
    assert fpath1 in folder_content
    assert folder_content[fpath1].is_blob()
    assert not folder_content[fpath1].is_folder()
    assert fpath2 in folder_content
    assert folder_content[fpath2].is_blob()
    assert not folder_content[fpath2].is_folder()
    assert sub_sub_path in folder_content
    assert not folder_content[sub_sub_path].is_blob()
    assert folder_content[sub_sub_path].is_folder()

    logger.info('Check that list of sub_sub folder is empty: %s', sub_sub_path)
    assert storage.list_folder(sub_sub_path) == {}

    logger.info('Check that listing content of a blob raises: %s', fpath1)
    try:
        storage.list_folder(fpath1)
        pytest.fail('Listing a blob should raise')
    except CInvalidFileTypeError as e:
        assert e.path == fpath1
        assert e.expected_blob is False

    logger.info('Delete file1: %s', fpath1)
    assert storage.delete(fpath1) is True  # We have deleted the file
    assert storage.delete(fpath1) is False  # We have not deleted anything

    logger.info('Check file1 does not appear anymore in folder: %s', sub_folder)
    assert fpath1 not in storage.list_folder(sub_folder)
    tmp = storage.get_file(fpath1)
    assert tmp is None

    logger.info('Delete whole test folder: %s', temp_root_path)
    ret = storage.delete(temp_root_path)
    assert ret is True  # We have deleted at least one file
    logger.info('Deleting again returns False')
    ret = storage.delete(temp_root_path)
    assert ret is False  # We have not deleted anything

    logger.info('Listing a deleted folder returns None: %s', temp_root_path)
    assert storage.list_folder(temp_root_path) is None
    assert storage.get_file(temp_root_path) is None


def test_blob_content_type(storage):
    if storage.provider_name() == 'dropbox':
        pytest.xfail('Dropbox ignores content type')
    if storage.provider_name() == 'rapidshare':
        pytest.xfail('Rapidshare ignores content type')
    if storage.provider_name() == 'googledrive':
        pytest.xfail('Google drive handles content type quite strangely')
    if storage.provider_name() == 'cloudme':
        pytest.xfail('CloudMe handles content type quite strangely')

    temp_root_path = miscutils.generate_test_path()
    try:
        logger.info('Will use test folder: %s', temp_root_path)

        # some providers are sensitive to filename suffix, so we do not specify any here:
        c_path = temp_root_path.add('uploaded_blob')
        data = b'some content...'
        content_type = 'text/plain; charset=Latin-1'
        upload_request = CUploadRequest(c_path, MemoryByteSource(data))
        upload_request.content_type(content_type)
        storage.upload(upload_request)

        c_file = storage.get_file(c_path)
        assert c_file.is_blob()
        assert c_file.content_type == content_type

        # Update file content, check content-type is updated also:
        data = b'some\x05binary\xFFcontent...'
        content_type = 'application/octet-stream'
        upload_request = CUploadRequest(c_path, MemoryByteSource(data))
        upload_request.content_type(content_type)
        storage.upload(upload_request)

        c_file = storage.get_file(c_path)
        assert c_file.content_type == content_type
    finally:
        storage.delete(temp_root_path)


def test_blob_metadata(storage):
    if storage.provider_name() == 'dropbox':
        pytest.xfail('Dropbox ignores metadata')
    if storage.provider_name() == 'rapidshare':
        pytest.xfail('Rapidshare ignores metadata')
    if storage.provider_name() == 'googledrive':
        pytest.xfail('Google Drive ignores metadata')
    if storage.provider_name() == 'cloudme':
        pytest.xfail('CloudMe handles ignores metadata')

    temp_root_path = miscutils.generate_test_path()
    try:
        logger.info('Will use test folder: %s', temp_root_path)
        c_path = temp_root_path.add('uploaded_blob.txt')
        data = b'some content...'
        upload_request = CUploadRequest(c_path, MemoryByteSource(data))
        metadata = { 'foo': 'this is foo...', 'bar': '1€ la bière' }
        upload_request.metadata(metadata)
        storage.upload(upload_request)

        c_file = storage.get_file(c_path)
        assert c_file.metadata == metadata
    finally:
        storage.delete(temp_root_path)


def test_delete_single_folder(storage):
    # We'll use a temp folder for tests:
    temp_root_path = miscutils.generate_test_path()
    logger.info('Will use test folder: %s', temp_root_path)

    # Create two sub folders: a/ and ab/:
    fpatha = temp_root_path.add('a')
    fpathab = temp_root_path.add('ab')
    storage.create_folder(fpatha)
    storage.create_folder(fpathab)
    assert storage.get_file(fpatha).is_folder()
    assert storage.get_file(fpathab).is_folder()

    c_path = fpatha.add('uploaded_blob.txt')
    data = b'some content...'
    upload_request = CUploadRequest(c_path, MemoryByteSource(data))
    storage.upload(upload_request)

    # Now delete folder a: folder ab should not be deleted
    storage.delete(fpatha)
    # It has been seen once a failed assertion below with google drive: get_file(fpatha) returns old folder
    # This is probably caused by different servers that are not always updated ?
    assert storage.get_file(fpatha) is None
    assert storage.get_file(c_path) is None
    assert storage.get_file(fpathab).is_folder()

    storage.delete(temp_root_path)


def test_invalid_file_operations(storage):
    # We'll use a temp folder for tests:
    temp_root_path = miscutils.generate_test_path()
    logger.info('Will use test folder: %s', temp_root_path)

    # Upload 1 file into this folder:
    fpath1 = temp_root_path.add('a_test_file1')
    content_file1 = b'This is binary cont€nt of test file 1...'
    upload_request = CUploadRequest(fpath1, MemoryByteSource(content_file1))
    storage.upload(upload_request)
    logger.info('Created blob: %s', fpath1)

    # Create a sub_folder:
    sub_folder = temp_root_path.add('sub_folder')
    storage.create_folder(sub_folder)

    logger.info('Check that listing content of a blob raises: %s', fpath1)
    try:
        storage.list_folder(fpath1)
        pytest.fail('Listing a blob should raise')
    except CInvalidFileTypeError as e:
        assert e.path == fpath1
        assert e.expected_blob is False

    logger.info('Check that trying to download a folder raises: %s', sub_folder)
    mbs = MemoryByteSink()
    download_request = CDownloadRequest(sub_folder, mbs)
    try:
        storage.download(download_request)
        pytest.fail('Downloading a folder should raise')
    except CInvalidFileTypeError as e:
        assert e.path == sub_folder
        assert e.expected_blob is True

    logger.info('Check that we cannot create a folder over a blob: %s', fpath1)
    try:
        storage.create_folder(fpath1)
        pytest.fail('Creating a folder over a blob should raise')
    except CInvalidFileTypeError as e:
        assert e.path == fpath1
        assert e.expected_blob is False

    logger.info('Check we cannot upload over an existing folder: %s', sub_folder)
    try:
        content = b'some data...'
        upload_request = CUploadRequest(sub_folder, MemoryByteSource(content))
        storage.upload(upload_request)
        pytest.fail('Uploading over a folder should raise')
    except CInvalidFileTypeError as e:
        assert e.path == sub_folder
        assert e.expected_blob is True

    logger.info('Check that content of a never existed folder is None')
    c_path = CPath('/hope i did never exist (even for tests) !')
    assert storage.list_folder(c_path) is None
    logger.info('Check that get_file() returns None is file does not exist')
    assert storage.get_file(c_path) is None

    logger.info('Check that downloading a non-existing file raises')
    dr = CDownloadRequest(c_path, MemoryByteSink())
    try:
        storage.download(dr)
        pytest.fail('Downlad a non-existing blob should raise')
    except CFileNotFoundError as e:
        logger.info('Expected exception: %s', e)
        assert e.path == dr.path

    storage.delete(temp_root_path)


def test_create_folder_over_blob(storage):
    # We'll use a temp folder for tests:
    temp_root_path = miscutils.generate_test_path()
    logger.info('Will use test folder: %s', temp_root_path)
    try:
        # Upload 1 file into this folder:
        fpath1 = temp_root_path.add('a_test_file1')
        content_file1 = b'This is binary cont€nt of test file 1...'
        upload_request = CUploadRequest(fpath1, MemoryByteSource(content_file1))
        storage.upload(upload_request)
        logger.info('Created blob: %s', fpath1)

        try:
            c_path = fpath1.add('sub_folder1')
            logger.info('Check we cannot create a folder when remote path traverses a blob: %s', c_path)
            storage.create_folder(c_path)
            if storage.provider_name() == 'dropbox':
                # This is known to fail on Dropbox:
                pytest.xfail('Creating folder when path contains a blob should raise')
            pytest.fail('Creating folder when path contains a blob should raise')
        except CInvalidFileTypeError as e:
            assert e.path == fpath1  # this is the problematic path
            assert e.expected_blob is False

    finally:
        storage.delete(temp_root_path)


def test_implicit_create_folder_over_blob(storage):
    # We'll use a temp folder for tests:
    temp_root_path = miscutils.generate_test_path()
    logger.info('Will use test folder: %s', temp_root_path)
    try:
        # Upload 1 file into this folder:
        fpath1 = temp_root_path.add('a_test_file1')
        content_file1 = b'This is binary cont€nt of test file 1...'
        upload_request = CUploadRequest(fpath1, MemoryByteSource(content_file1))
        storage.upload(upload_request)
        logger.info('Created blob: %s', fpath1)

        # Uploading blob will implicitely create intermediary folders,
        # so will try to erase fpath1
        try:
            c_path = fpath1.add('sub_file1')
            logger.info('Check we cannot upload a blob when remote path traverses a blob: %s', c_path)
            content = b'some data...'
            upload_request = CUploadRequest(c_path, MemoryByteSource(content))
            storage.upload(upload_request)
            if storage.provider_name() == 'dropbox':
                # This is known to fail on Dropbox:
                pytest.xfail('Uploading when path contains a blob should raise')
            pytest.fail('Uploading when path contains a blob should raise')
        except CInvalidFileTypeError as e:
            assert e.path == fpath1  # this is the problematic path
            assert e.expected_blob is False

    finally:
        storage.delete(temp_root_path)


def test_file_with_special_chars(storage):
    temp_root_path = miscutils.generate_test_path()
    try:

        folder_path = temp_root_path.add("hum...\u00a0',;.:\u00a0!*%&~#{[|`_ç^@ £€")
        storage.create_folder(folder_path)
        fback = storage.get_file(folder_path)
        assert fback.path == folder_path
        assert fback.is_folder()
        assert not fback.is_blob()

        # Folder must appear in test root folder list:
        root_test_content = storage.list_folder(temp_root_path)
        assert folder_path in root_test_content
        assert root_test_content[folder_path].path == folder_path
        assert root_test_content[folder_path].is_folder()
        assert not root_test_content[folder_path].is_blob()

        # Generate a random blob name (ensure it does not start nor end with a space)
        nameb = list()
        nameb.append('b')
        for i in range(0,30):
            nameb.append(_generate_random_blob_name_char(storage.provider_name()))
        nameb.append('e')
        blob_path = None
        last_blob_path = None
        for i in range(0,10):
            last_blob_path = blob_path
            # slightly change blob name, so that we get similar but different names:
            index = int(random.random() * (len(nameb) - 2)) + 1
            nameb[index] = _generate_random_blob_name_char(storage.provider_name())
            blob_name = ''.join(nameb)
            blob_path = folder_path.add(blob_name)
            logger.info("Will upload file to path: %r", blob_path)

            content_file = b'This is cont€nt of test file: ' + blob_name.encode('UTF-8')
            upload_request = CUploadRequest(blob_path, MemoryByteSource(content_file))
            upload_request.content_type('text/plain; charset=UTF-8')
            storage.upload(upload_request)
            bback = storage.get_file(blob_path)
            if bback is None:
                logger.error('Test failed with blob_path=%r', blob_path)
                logger.error('            last_blob_path=%r', last_blob_path)
            assert bback is not None
            assert bback.path == blob_path
            assert bback.is_blob()
            assert not bback.is_folder()

            # Download and check content:
            mbs = MemoryByteSink()
            download_request = CDownloadRequest(blob_path, mbs)
            # It has been seen once a 404 error below with google drive:
            # This may be caused by different servers that are not yet informed of file creation ?
            storage.download(download_request)
            mbs.get_bytes() == content_file

            # blob must appear in folder list:
            folder_content = storage.list_folder(folder_path)
            assert blob_path in folder_content
            assert folder_content[blob_path].path == blob_path
            assert folder_content[blob_path].is_blob()
            assert not folder_content[blob_path].is_folder()

    finally:
        storage.delete(temp_root_path)


def _generate_random_blob_name_char(provider_name):
    """ Generate a character compatible with provider.

    TODO refactor this: each provider class should specify unacceptable characters ?
    """
    while True:
        codepoint = int(random.random() * random.random() * 200 + 32)
        if random.random() < 0.01:
            codepoint = ord('€')  # quite longer once UTF-8 encoded
        if codepoint >= 127 and codepoint < 160:
            continue
        c = unichr(codepoint)
        if c == '/' or c == '\\':
            continue;
        # CloudMe nor rapidshare providers does not support quotes so we do not use them:
        if c in '"' and provider_name == 'cloudme':
            continue;
        if (c in '\'",:@><*?`^') and provider_name == 'rapidshare':
            continue;
        if ord(c) > 127 and provider_name == 'rapidshare':
            continue;
        return c;


def test_abort_during_download(storage, tmpdir):
    """If download fails, check that abort is called on progress listener"""
    # Upload a quite big file (for download):
    file_size = 500000
    c_path = miscutils.generate_test_path()
    logger.info('Uploading blob with size %d bytes to %s', file_size, c_path)
    try:
        logger.info('Will upload a blob for download test (%d bytes) to %s', file_size, c_path)
        content = bytearray(os.urandom(file_size))
        upload_request = CUploadRequest(c_path, MemoryByteSource(content))
        storage.upload(upload_request)

        # Now we download, asking for progress:
        logger.info('Will download this blob but fail during download...')
        local_pathname = tmpdir.join('/back_from_provider').strpath
        fbs = FileByteSink(local_pathname, temp_name_during_writes=False, delete_on_abort=True)
        pl = TestAbortProgressListener(1,  # abort once
                                       file_size / 2,  # abort at half download
                                       False)  # error is not temporary
        pbs = ProgressByteSink(fbs, pl)
        dr = CDownloadRequest(c_path, pbs)
        with pytest.raises(RuntimeError) as rte:
            storage.download(dr)
            pytest.fail("Download should have failed !")
        logger.info('Download has failed (as expected)')
        # Check listener saw the abort:
        assert pl.is_aborted
        # check file does not exist (has been removed):
        logger.info('Check destination file does not exist: %s', local_pathname)
        assert not os.path.exists(local_pathname)
    finally:
        if c_path:
            storage.delete(c_path)


def test_abort_twice_during_upload(storage, tmpdir):
    """Check upload is properly retried in case of temporary failure"""
    file_size = 500000
    c_path = miscutils.generate_test_path()
    logger.info('Uploading blob with size %d bytes to %s', file_size, c_path)
    try:
        logger.info('Will upload a blob for test (%d bytes) to %s, but fail temporarily during first two uploads', file_size, c_path)
        content = bytearray(os.urandom(file_size))
        pl = TestAbortProgressListener(2,  # fail twice
                                       file_size / 2,  # abort at half upload
                                       True )
        pbs = ProgressByteSource( MemoryByteSource(content), pl)
        upload_request = CUploadRequest(c_path, pbs)
        storage.upload(upload_request)

        # check file exists:
        logger.info('Check uploaded file: %s', c_path)
        c_file = storage.get_file(c_path)
        assert c_file is not None
        assert c_file.length == file_size

    finally:
        if c_path:
            storage.delete(c_path)


class TestAbortProgressListener(StdoutProgressListener):
    """A progress listener that raises at specified offset progress, a specified number of times."""
    def __init__(self, nbFails, offset_limit, retriable):
        super(TestAbortProgressListener, self).__init__()
        self.nbFailsTotal = nbFails
        self.limit = offset_limit
        self.retriable = retriable
        self.nbFailsCurrent = 0

    def progress(self, current):
        super(TestAbortProgressListener, self).progress(current)
        if current >= self.limit:
            if self.nbFailsCurrent < self.nbFailsTotal:
                self.nbFailsCurrent += 1
                ex = RuntimeError('This is a test error to make up/download fail...')
                if self.retriable:
                    ex = CRetriableError(ex)
                raise ex


