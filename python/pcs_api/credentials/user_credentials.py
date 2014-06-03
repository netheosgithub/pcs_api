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


class UserCredentials(object):
    """This class holds user_id and credentials (password or OAuth2 tokens).
    Credentials is a dictionary here.
    """

    def __init__(self, app_info, user_id, credentials):
        self.app_info = app_info
        self.user_id = user_id
        self._credentials = credentials

    def credentials(self):
        return self._credentials

    def set_new_credentials(self, new_credentials):
        self._credentials = new_credentials

    def __repr__(self):
        return "UserCredentials(user=%r) for app %r.%r" % \
               (self.user_id, self.app_info.provider_name, self.app_info.app_name)
