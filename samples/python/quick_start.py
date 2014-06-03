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
import argparse

# Required for providers registration :
from pcs_api.providers import *
from pcs_api.storage import StorageFacade
from pcs_api.credentials.app_info_file_repo import AppInfoFileRepository
from pcs_api.credentials.user_creds_file_repo import UserCredentialsFileRepository
from pcs_api.bytes_io import (MemoryByteSource, MemoryByteSink,
                              FileByteSource, FileByteSink,
                              StdoutProgressListener)
from pcs_api.models import CPath, CFolder, CBlob, CUploadRequest, CDownloadRequest

parser = argparse.ArgumentParser(description='Quick test of storage API.',
                                 epilog='AppInfoRepository and UserCredentialsRepository must be ready.')
parser.add_argument('provider_name', help='provider name')
parser.add_argument('-a', '--app_name',
                    help='application name, as registered provider-side. '
                         'If not supplied, app repos should contain a single application for given provider')
parser.add_argument('-u', '--user_id',
                    help='user login or email. '
                         'If not supplied, user_creds repos should contain a single user for given application')
parser.add_argument('-v', '--verbose', action='count',
                    help='Increase logging level. '
                         'Several -v options may be provided (this may leak confidential informations).')
parser.add_argument('-w', '--wire', action='store_const', const=True,
                    help='Logs http requests (leaks confidential informations).')
cli_args = parser.parse_args()

if cli_args.verbose >= 1:
    logging.basicConfig(level=logging.DEBUG if cli_args.verbose >= 2 else logging.INFO)
    if not cli_args.wire:
        log = logging.getLogger("urllib3")
        log.setLevel(logging.INFO)
        log = logging.getLogger("oauthlib")
        log.setLevel(logging.INFO)

if cli_args.wire:
    import httplib
    httplib.HTTPConnection.debuglevel = 1
    requests_log = logging.getLogger("requests.packages.urllib3")
    requests_log.setLevel(logging.DEBUG)
    requests_log.propagate = True

apps_repo = AppInfoFileRepository("../../repositories/app_info_data.txt")
user_credentials_repo = UserCredentialsFileRepository("../../repositories/user_credentials_data.txt")

storage = StorageFacade.for_provider(cli_args.provider_name) \
                       .app_info_repository(apps_repo, cli_args.app_name) \
                       .user_credentials_repository(user_credentials_repo, cli_args.user_id) \
                       .build()

print("user_id = ", storage.get_user_id())
print("quota = ", storage.get_quota())

# Recursively list all regular files (blobs in folders) :
folders_to_process = [CPath('/')]
largest_blob = None
while folders_to_process:
    f = folders_to_process.pop()
    # list folder :
    print("\nContent of %s :" % f)
    content = storage.list_folder(f)
    blobs = [f for f in content.values() if f.is_blob()]
    folders = [f for f in content.values() if f.is_folder()]
    # print blobs of this folder :
    for b in blobs:
        print("  ", b)
        if (not largest_blob) or b.length > largest_blob.length:
            largest_blob = b
    for f in folders:
        print("  ", f)
    # Add folders to list :
    folders_to_process += folders

# Create a folder and upload a test file :
root_folder_content = storage.list_root_folder()
# root_folder_content is a dict
# keys are files paths, values are CFolder or CBlob providing details (length, content-type...)
print('root folder content :', root_folder_content)

# Create a new folder :
fpath = CPath('/pcs_api_new_folder')
storage.create_folder(fpath)

# Upload a local file in this folder :
bpath = fpath.add('pcs_api_new_file')
file_content = b'this is file content...'
upload_request = CUploadRequest(bpath, MemoryByteSource(file_content)).content_type('text/plain')
storage.upload(upload_request)

# Download back the file :
mbs = MemoryByteSink()
download_request = CDownloadRequest(bpath, mbs)
storage.download(download_request)
assert mbs.get_bytes() == file_content

# delete remote folder :
storage.delete(fpath)

# Example : we'll download a range of largest blob :
if largest_blob:
    range_start = largest_blob.length / 2
    range_length = min(largest_blob.length / 2, 1000000)
    bs = FileByteSink('dest_file.txt')
    print("Will download range from largest blob : %s to : %s" %
          (largest_blob, bs))
    dr = CDownloadRequest(largest_blob.path, bs).range(range_start, range_length)
    # watch download progress :
    dr.progress_listener(StdoutProgressListener())
    storage.download(dr)
