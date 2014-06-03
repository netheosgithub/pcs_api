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
import random
import threading

from pcs_api.storage import StorageFacade
from pcs_api.bytes_io import StdoutProgressListener
from pcs_api.models import CPath, CFolder, CBlob, CUploadRequest, CDownloadRequest
from pcs_api.bytes_io import MemoryByteSource, MemoryByteSink

logger = logging.getLogger(__name__)
# Import tests utility functions & configuration, etc.
import miscutils


def test_crud(storage, duration):
    # rapidshare free accounts can be locked if duration is too large:
    # TODO: would better to limit the number of requests instead of duration
    rapid_share_limit = 300
    if storage.provider_name() == 'rapidshare' and duration > rapid_share_limit:
        logger.warn('test_crud() with rapidshare will be limited to %d seconds (duration=%d)'
                    % (rapid_share_limit, duration))
        duration = rapid_share_limit

    start = time.time()
    logger.info('Test starting time=%f', start)
    now = start
    # if start > now then time has gone back:
    # we give up instead of being trapped in a potentially very long loop
    while start <= now and now-start < duration:
        logger.info('============= Thread %r: (elapsed=%f < %d seconds) ================',
                    threading.current_thread(), now-start, duration)
        test_root_path = miscutils.generate_test_path()
        storage.get_user_id()  # only to request hubic API
        upload_and_check_random_files(storage, test_root_path)
        now = time.time()


def test_multi_crud(nb_thread, storage, duration):
    """Multi-threaded test of test_crud()."""
    miscutils.test_in_threads(nb_thread, storage.provider_name(), test_crud, storage, duration)


def recursively_list_blobs(storage, c_path):
    ret_list = []
    files = storage.list_folder(c_path)
    for f in files.values():
        if f.is_blob():
            ret_list.append(f)
        else:
            ret_list += recursively_list_blobs(storage, f.path)
    return ret_list


def upload_and_check_random_files(storage, root_path):
    rnd = random.Random()  # not shared (in case multi-thread)
    tmp_path = CPath(root_path.path_name())
    for i in range(0, rnd.randint(1, 5)):
        c_path = miscutils.generate_test_path(tmp_path)

        if rnd.randint(0, 2) == 0:
            # Create folder:
            storage.create_folder(c_path)
            # And sometimes go inside:
            if rnd.randint(0, 2) == 0:
                tmp_path = c_path
        else:
            # Create file (deterministic content for later check):
            rnd.seed(c_path)
            file_size = rnd.randint(0, 1000) * rnd.randint(0, 1000)  # prefer small files
            data = miscutils.generate_random_bytearray(file_size, rnd)
            ur = CUploadRequest(c_path, MemoryByteSource(data))
            storage.upload(ur)

    # Check blob files content:
    all_blobs = recursively_list_blobs(storage, root_path)
    logger.info('All uploaded blobs = %s', all_blobs)
    for blob in all_blobs:
        rnd.seed(blob.path)
        file_size = rnd.randint(0, 1000) * rnd.randint(0, 1000)  # same formula as above
        assert file_size == blob.length
        expected_data = miscutils.generate_random_bytearray(file_size, rnd)
        bsink = MemoryByteSink()
        dr = CDownloadRequest(blob.path, bsink)
        storage.download(dr)
        downloaded_data = bsink.get_bytes()
        # In order to track strange download errors:
        if expected_data != downloaded_data:
            logger.error('Downloaded data differ (%d / %d bytes): will download again for disgnostic',
                         len(expected_data), len(downloaded_data))
            bsink2 = MemoryByteSink()
            dr2 = CDownloadRequest(blob.path, bsink2)
            storage.download(dr2)
            downloaded_data2 = bsink2.get_bytes()
            logger.error('Second download: (%d bytes). Data are the same now?=%r / Same as first download?=%r',
                         len(downloaded_data2),
                         expected_data == downloaded_data2,
                         downloaded_data == downloaded_data2)
        # py.test cannot really compare bytearray and bytes, so we convert for nicer display:
        assert bytes(expected_data) == downloaded_data

    # Delete all files:
    storage.delete(root_path)

