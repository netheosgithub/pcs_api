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


class OAuth2ProviderParameters(object):
    """This is just a basic object that stores provider endpoints URL for OAuth2,
    and is able to take into account some variations between providers"""

    def __init__(self,
                 authorize_url,
                 access_token_url,
                 refresh_token_url=None,
                 scope_in_authorization=False, scope_perms_separator=None):
        super(OAuth2ProviderParameters, self).__init__()
        self.authorize_url = authorize_url
        self.access_token_url = access_token_url
        self.refresh_token_url = refresh_token_url
        self.scope_in_authorization = scope_in_authorization
        self.scope_perms_separator = scope_perms_separator

    def scope_for_authorization(self, scope):
        """Convert scope (list of permissions) to string for building OAuth authorization URL.

        Some providers separate permissions with spaces, other with comma... Other do not support scope at all.
        """
        if self.scope_in_authorization:
            return self.scope_perms_separator.join(scope)
        return None

    def granted_scope(self, permissions_str):
        """When app is authorized, callback URL may contain granted permissions
        (ex: hubic).

        This method converts the returned permissions string into a list to get back a scope.
        FIXME check unicode / byte string, different order, etc.
        Comparing scopes is not always feasible
        """
        scope = permissions_str.split(self.scope_perms_separator)
        if scope == ['']:
            scope = []
        return scope

