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
import os

# Required for all providers registration:
from pcs_api.providers import *
import pcs_api.storage
from pcs_api.storage import StorageFacade
from pcs_api.credentials.app_info_file_repo import AppInfoFileRepository
from pcs_api.credentials.user_creds_file_repo import UserCredentialsFileRepository


logger = logging.getLogger(__name__)


# Command line options to configure functional tests:
def pytest_addoption(parser):
    parser.addoption("--providers", default="all",
                     help="Set comma separated provider(s) name(s) for tests, or 'all' (default is 'all')")
    parser.addoption("--test-duration", default="60", type=int,
                     help="Duration of stress tests, in seconds")
    parser.addoption("--nb_thread", default="4", type=int,
                     help="Number of threads for multi-threads tests")


def pytest_generate_tests(metafunc):
    """Generate test parameters according to command line options."""
    if 'storage' in metafunc.fixturenames:
        if metafunc.config.option.providers == 'all':
            providers_names = StorageFacade.get_registered_providers()
        else:
            providers_names = metafunc.config.option.providers.split(',')
        # Instantiate one storage for each provider, if possible:
        storages = []
        if apps_repo:
            logger.info("Using providers names = %r", providers_names)
            for provider_name in providers_names:
                storage = StorageFacade.for_provider(provider_name) \
                    .app_info_repository(apps_repo, None) \
                    .user_credentials_repository(user_credentials_repo, None) \
                    .build()
                storages.append(storage)
        else:
            # Missing apps_repo, etc. Functional tests can not run
            logger.warn("No provider can be instantiated")
        metafunc.parametrize("storage", storages)


@pytest.fixture
def duration(request):
    val = request.config.getoption("--test-duration")
    # for unknown reasons (current workking dir, pytest other options...),
    # sometimes val is a unicode here ?
    return int(val)


@pytest.fixture
def nb_thread(request):
    val = request.config.getoption("--nb_thread")
    return int(val)


# TODO make this parametrizable
logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s [%(threadName)s] %(name)-12s %(levelname)-8s %(message)s',)
pcs_api_log = logging.getLogger('pcs_api')
pcs_api_log.setLevel(logging.DEBUG)
pcs_api_log.propagate = True
for name in ['requests.packages.urllib3',
             'requests.packages.urllib3.connectionpool',
             'oauthlib']:
    log = logging.getLogger(name)
    log.setLevel(logging.DEBUG)

#oauthlib_log = logging.getLogger()
#oauthlib_log.setLevel(logging.INFO)

#pcs_api_log.setLevel(logging.DEBUG)

#log = logging.getLogger("urllib3")
#log.setLevel(logging.DEBUG)
#requests_log.propagate = True
#import httplib
#httplib.HTTPConnection.debuglevel = 1

logger.info("Testing with pcs_api version = %s", pcs_api.__version__)

repository_dir_env_var_name = 'PCS_API_REPOSITORY_DIR'

# Instantiate file repositories for app and users, if possible:
file_repositories_folder = os.environ.get(repository_dir_env_var_name)
if not file_repositories_folder:
    file_repositories_folder = '../repositories'
    logger.info('Environment variable %s is not defined: using %s',
                repository_dir_env_var_name, file_repositories_folder)
apps_repo_pathname = "%s/app_info_data.txt" % file_repositories_folder
if os.path.exists(apps_repo_pathname):
    apps_repo = AppInfoFileRepository("%s/app_info_data.txt" % file_repositories_folder)
    user_credentials_repo = UserCredentialsFileRepository("%s/user_credentials_data.txt" % file_repositories_folder)

    logger.info("Functional tests will use Apps Repos: %r", apps_repo)
    logger.info("Functional tests will use UserCredentials Repos: %r", user_credentials_repo)
else:
    logger.warn('App info file repository not found for functional tests: %s'
                '\nSet PCS_API_REPOSITORY_DIR environment variable', apps_repo_pathname)
    logger.warn('No functional test will be run')
    apps_repo = None

# For checking modification times: how much difference do we allow ?
# TODO: one may use a NTP reference here, hoping providers are time synchronized
TIME_ALLOWED_DELTA_s = 120
