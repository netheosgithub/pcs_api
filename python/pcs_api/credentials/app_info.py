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


class AppInfo(object):
    """This class holds application informations for OAuth2 providers (web application workflow).
    Also used in case of login/password authenticated users: in this case application name
    can be set to 'login'
    """

    def __init__(self, provider_name, app_name, app_id=None, app_secret=None, scope=None, redirect_url=None):
        """
        :param provider_name:  REQUIRED. lower case name of provider

        :param app_name:   REQUIRED. Name of application, as registered on provider.
                                    'login' may be used for providers with login/pw authentication.

        :param app_id: OPTIONAL. If OAuth provider, application ID

        :param app_secret OPTIONAL. If OAuth provider, application secret

        :param scope OPTIONAL. If OAuth provider, list of permissions asked by application

        :param redirect_url OPTIONAL. If OAuth provider, application URL (for web application workflow)
        """
        self.provider_name = provider_name
        self.app_name = app_name
        self.app_id = app_id
        self.app_secret = app_secret
        if scope and not isinstance(scope, (list, tuple)):
            raise ValueError("Invalid scope: not a list")
        self.scope = scope
        self.redirect_url = redirect_url

    def is_oauth(self):
        """Returns True if this is an OAuth registered application.
        (ie iff app_id is not None)"""
        return self.app_id is not None

    def __repr__(self):
        return "AppInfo(%s.%s, app_id=%r, scope=%r, redirect=%r)" % \
               (self.provider_name, self.app_name, self.app_id, self.scope, self.redirect_url)

