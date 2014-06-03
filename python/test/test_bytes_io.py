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
import os

from pcs_api.bytes_io import (
    FileByteSource, MemoryByteSource,
    RangeByteSource, ProgressByteSource,
    FileByteSink, MemoryByteSink,
    ProgressByteSink, ProgressListener, StdoutProgressListener)

logger = logging.getLogger(__name__)


def test_file_byte_source(tmpdir):
    str_content = 'This 1€ file is the test content of a file byte source... (70 bytes)'
    byte_content = str_content.encode('UTF-8')
    test_file = tmpdir.join('byte_source.txt')
    test_file.write(byte_content, 'wb')
    test_file_path = test_file.strpath

    bs = FileByteSource(test_file_path)
    check_byte_source(bs, byte_content)

    bs = MemoryByteSource(byte_content)
    check_byte_source(bs, byte_content)


def check_byte_source(byte_source, expected_content):
    assert byte_source.length() == 70
    with byte_source.open_stream() as in_stream:
        b = in_stream.read()
        assert b == expected_content
        assert in_stream.closed is False
    assert in_stream.closed is True

    # Create a range view of this byte source: 25 bytes starting at offset 5:
    rbs = RangeByteSource(byte_source, 5, 25)
    assert rbs.length() == 25
    in_stream = rbs.open_stream()
    try:
        b = in_stream.read(1)
        assert b == b'1'
        b = in_stream.read(3)
        assert b == '€'.encode('UTF-8')
        b = in_stream.read(100)
        assert len(b) == 21
        assert b == ' file is the test con'
        b = in_stream.read(100)
        assert len(b) == 0
        assert in_stream.closed is False
    finally:
        in_stream.close()
    assert in_stream.closed is True

    # Now decorate again with a progress byte source:
    pl = StdoutProgressListener()
    pbs = ProgressByteSource(rbs, pl)
    assert pbs.length() == rbs.length()
    in_stream = pbs.open_stream()
    try:
        assert pl.total == pbs.length()
        assert pl.current == 0
        assert pl.is_aborted is False
        assert in_stream.closed is False
        assert in_stream.readable() is True
        assert in_stream.writable() is False
        b = in_stream.read(1)
        assert pl.current == 1
        b = in_stream.read(10)
        assert pl.current == 11
        b = bytearray(500)
        n = in_stream.readinto(b)
        assert n == 14
        assert pl.current == 25
    finally:
        in_stream.close()
    assert in_stream.closed is True


def test_file_byte_sink(tmpdir):
    str_content = 'This 1€ file is the test content of a file byte sink...   (70 bytes)'
    byte_content = str_content.encode('UTF-8')
    test_file = tmpdir.join('byte_sink.txt')
    test_file_path = test_file.strpath

    check_file_byte_sink_all_flags(test_file_path, byte_content)
    check_memory_byte_sink(byte_content)

def check_file_byte_sink_all_flags(test_file_path, byte_content):
    for abort in [True,False]:
        for temp_name_during_writes in [True,False]:
            for delete_on_abort in [True,False]:
                fbs = FileByteSink(test_file_path,
                                   temp_name_during_writes, delete_on_abort)
                check_file_byte_sink( byte_content, abort, fbs, test_file_path,
                                      temp_name_during_writes, delete_on_abort)


def test_progress_byte_sink(tmpdir):
    byte_content = 'hello world !'.encode('UTF-8')
    test_file = tmpdir.join('byte_sink_progress.txt')
    test_file_path = test_file.strpath

    fbs = FileByteSink(test_file_path)
    check_progress_byte_sink(fbs, byte_content)

    mbs = MemoryByteSink()
    check_progress_byte_sink(mbs, byte_content)


def check_progress_byte_sink(byte_sink, byte_content):
    pl = StdoutProgressListener()
    pbs = ProgressByteSink(byte_sink, pl)
    out_stream = pbs.open_stream()
    try:
        assert pl.total is None
        assert pl.current == 0
        assert pl.is_aborted is False
        pbs.set_expected_length(len(byte_content))
        assert pl.total == len(byte_content)
        out_stream.write(byte_content[0:1])
        assert pl.current == 1
        out_stream.write(byte_content[1:])
        assert pl.current == pl.total
        out_stream.abort()
        assert pl.is_aborted
    finally:
        out_stream.close()


def check_file_byte_sink(data_to_write, abort, fbs, pathname, with_temp_name, delete_on_abort):
    actual_pathname = pathname if not with_temp_name else pathname+'.part'
    out_stream = fbs.open_stream()
    try:
        # check file exists:
        assert os.path.exists(actual_pathname)
        fbs.set_expected_length(len(data_to_write))
        # write not all bytes, only beginning of data:
        out_stream.write(data_to_write[0:10])
        out_stream.flush()
        assert os.path.getsize(actual_pathname) == 10
        if abort:
            logger.info('Aborting stream of byte sink !')
            out_stream.abort()
    finally:
        out_stream.close()
    file_still_exists = os.path.exists(pathname)
    temp_file_still_exists = os.path.exists(actual_pathname)
    if not abort:
        # operation has not been aborted: so file never deleted
        assert file_still_exists
    else:
        # operation has been aborted:
        # if a temp file is used and not deleted, it has not been renamed:
        assert temp_file_still_exists is not delete_on_abort


def check_memory_byte_sink(byte_content):
    byte_sink = MemoryByteSink()
    stream = byte_sink.open_stream()
    try:
        stream.write(byte_content)
    finally:
        stream.close()
    stored_data = byte_sink.get_bytes()
    assert byte_content == stored_data
