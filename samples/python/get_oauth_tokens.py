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

# OAuth2 tokens bootstrapper: retrieving tokens manually
# (we usually fetch refresh_tokens, if provider supports these ;
# if not, access_token are returned but with a long lifetime)
#
# This small utility program is an example that shows
# how to populate a UserCredentialsRepository

from __future__ import absolute_import, unicode_literals, print_function
import argparse

from pcs_api.credentials.app_info_file_repo import AppInfoFileRepository
from pcs_api.credentials.user_creds_file_repo import UserCredentialsFileRepository
from pcs_api.credentials.user_credentials import UserCredentials
from pcs_api.oauth.oauth2_bootstrap import OAuth2BootStrapper
from pcs_api.storage import StorageFacade
# Required for registering providers :
from pcs_api.providers import *

parser = argparse.ArgumentParser(description='Manual OAuth authorization, to get refresh tokens.',
                                 epilog='AppInfoRepository must be ready; '
                                        'UserCredentialsRepository will be populated')
parser.add_argument('provider_name', help='provider name')
parser.add_argument('-a', '--app_name',
                    help='application name, as registered provider-side. '
                         'If not supplied, app repos should contain a single application for given provider')
cli_args = parser.parse_args()

# Here we use basic file repositories :
apps_repo = AppInfoFileRepository("../../repositories/app_info_data.txt")
user_credentials_repo = UserCredentialsFileRepository("../../repositories/user_credentials_data.txt")

import logging
import httplib
debug = True
if debug:
    httplib.HTTPConnection.debuglevel = 1
    logging.basicConfig(level=logging.DEBUG)
    requests_log = logging.getLogger("requests.packages.urllib3")
    requests_log.setLevel(logging.DEBUG)
    requests_log.propagate = True

# Check it is really an oauth app :
app_info = apps_repo.get(cli_args.provider_name, cli_args.app_name)
if not app_info.is_oauth():
    raise ValueError('This application does not use OAuth : %s' % app_info)
# Instantiate storage :
storage = StorageFacade.for_provider(cli_args.provider_name) \
    .app_info_repository(apps_repo, cli_args.app_name) \
    .user_credentials_repository(user_credentials_repo) \
    .for_bootstrap() \
    .build()
bootstrapper = OAuth2BootStrapper(storage)
bootstrapper.do_code_workflow()
