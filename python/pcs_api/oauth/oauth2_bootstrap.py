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


# OAuth2 tokens bootstrapper:
# manually retrieving tokens
# (we usually fetch refresh_tokens, if provider supports these ;
# if not, access_token are returned but with a long lifetime)
#
# This small utility program is to be used for bootstrapping UserCredentialsRepository

from __future__ import absolute_import, unicode_literals, print_function


class OAuth2BootStrapper(object):
    """Utility class to retrieve initial token and populate a UserCredentialsRepository"""

    def __init__(self, storage):
        self._storage = storage

    def do_code_workflow(self):
        url, state = self._storage._session_manager.get_authorize_url()
        print("Authorize URL:\n\n%s\n\n" % url)
        print("Copy paste in browser, authorize, then input full callback URL or authorization code:")
        code_or_url = raw_input()
        user_credentials = self._storage._session_manager.fetch_user_credentials(code_or_url, state)

        # (user_id is still unknown in UserCredentials, so we won't be able to save it yet)
        # So at first we have to retrieve user_id:
        user_id = self._storage.get_user_id()
        print("Retrieved user_id = ", user_id)

        user_credentials.user_id = user_id

        # By now we can save credentials:
        self._storage._session_manager._user_credentials_repository.save(user_credentials)
