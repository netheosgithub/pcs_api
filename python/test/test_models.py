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
import pytest

from pcs_api.models import CPath, CDownloadRequest, CUploadRequest
from pcs_api.bytes_io import MemoryByteSink, MemoryByteSource, StdoutProgressListener

logger = logging.getLogger(__name__)


def test_cpath():
    p = CPath('/foo//bar€/')
    assert p.path_name() == '/foo/bar€'
    assert p.url_encoded() == '/foo/bar%E2%82%AC'
    assert p.base_name() == 'bar€'
    assert p.parent() == CPath('/foo')
    assert p.add('a,file...') == CPath('/foo/bar€/a,file...')
    assert p.add('/a,file...') == CPath('/foo/bar€/a,file...')
    assert p.add('a,file.../') == CPath('/foo/bar€/a,file...')
    assert p.add('/several//folders/he re/') == CPath('/foo/bar€/several/folders/he re')
    assert p.is_root() is False
    assert p.parent().is_root() is False
    root = p.parent().parent()
    assert root.is_root() is True
    assert root.parent().is_root() is True
    assert root == CPath('/')
    assert root == CPath('')
    assert root.base_name() == ''
    assert root.split() == []
    assert CPath('').split() == []
    assert CPath('/a').split() == ['a']
    assert CPath('/alpha/"beta').split() == ['alpha', '"beta']


def test_cpath_as_key():
    adict = {CPath('/a'): 'file_a', CPath('/a/b'): 'file_b'}
    assert CPath('/a') in adict
    assert adict[CPath('a')] == 'file_a'
    assert CPath('/a/b') in adict
    assert adict[CPath('/a/b')] == 'file_b'
    assert CPath('/b') not in adict


def test_invalid_cpath():
    for pathname in ['\\no anti-slash is allowed',
                     'This is an inv\\u001Flid pathname !',
                     'This is an \t invalid pathname !'
                     'This/ is/an invalid pathname !',
                     'This/is /also an invalid pathname !',
                     ' bad', 'bad ', '\u00a0bad', 'bad\u00a0']:
        with pytest.raises(ValueError) as e:
            logger.info("Checking CPath is invalid: %s", pathname)
            p = CPath(pathname)
        logger.info("CPath validation failed as expected: %s", e)

def test_cpath_url_encoded():
    assert CPath('/a +%b/c').url_encoded() == '/a%20%2B%25b/c'
    assert CPath('/a:b').url_encoded() == '/a%3Ab'
    assert CPath('/€').url_encoded() == '/%E2%82%AC'


def test_download_request_bytes_range():
    dr = CDownloadRequest(CPath('/foo'), MemoryByteSink())
    dr.range(None, None)
    assert dr.get_http_headers() == {}
    dr.range(None, 100)
    assert dr.get_http_headers() == { 'Range': 'bytes=-100'}
    dr.range(10, 100)
    assert dr.get_http_headers() == { 'Range': 'bytes=10-109'}
    dr.range(100, None)
    assert dr.get_http_headers() == { 'Range': 'bytes=100-'}

def test_download_request_progress_listener():
    mbs = MemoryByteSink()
    dr = CDownloadRequest(CPath('/foo'), mbs)
    assert dr.byte_sink() is mbs

    # Now if we decorate:
    pl = StdoutProgressListener()
    dr.progress_listener(pl)
    os = dr.byte_sink().open_stream()
    os.write(b'a')
    assert pl.current == 1

def test_upload_request_progress_listener():
    mbs = MemoryByteSource(b'content')
    ur = CUploadRequest(CPath('/foo'), mbs)
    assert ur.byte_source() is mbs

    # Now if we decorate:
    pl = StdoutProgressListener()
    ur.progress_listener(pl)
    istream = ur.byte_source().open_stream()
    data = istream.read(1)
    assert pl.current == 1




