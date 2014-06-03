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
import json

from .app_info import AppInfo


class AppInfoFileRepository:
    """This class is a simple reader of applications informations
    from a plain text file with format:
    provider_name_1,app_name_1 = json_object
    provider_name_2,app_name_2 = json_object"""

    def __init__(self, data_filename):
        self._app_info = {}
        # Read all apps into our dict
        with open(data_filename, "r") as data_file:
            for line in data_file:
                line = line.strip()
                if not line or line[0] == '#':  # ignore empty or comment lines
                    continue
                # split at first = sign ; left is provider and app name, right is json app info
                prov_app_app_info = line.split('=', 1)
                if len(prov_app_app_info) != 2:
                    raise ValueError("Not parsable line: "+line)
                provider_name, app_name = prov_app_app_info[0].strip().split(".")
                str_info = prov_app_app_info[1].strip()
                #print ("str_info=", str_info)
                json_info = json.loads(str_info)
                if 'appId' in json_info: # is this an OAuth2 provider ?
                    app_info = AppInfo(provider_name, app_name,
                                       json_info['appId'], json_info['appSecret'],
                                       json_info['scope'],
                                       json_info.get('redirectUrl', None))
                else:
                    app_info = AppInfo(provider_name, app_name)
                self._app_info[app_key(provider_name, app_name)] = app_info

    def get(self, provider_name, app_name=None):
        """Retrieve application informations for the specified provider and optional application.

        If repository contains only one application for the specified provider, app_name may
        be left unspecified."""
        if app_name is not None:
            try:
                return self._app_info[app_key(provider_name, app_name)]
            except KeyError:
                raise ValueError("No application found for provider '%s' and name '%s'"
                                 % (provider_name, app_name))
        # If app_name is not specified, this is acceptable
        # if our repository contains only 1 app for this provider:
        app_info = None
        for k in self._app_info:
            if k.startswith(provider_name+'.'):
                if app_info is None:
                    app_info = self._app_info[k]
                else:
                    raise ValueError('Several applications found for provider: %s' % provider_name)
        if app_info is None:
            raise ValueError('No application found for provider: %s' % provider_name)
        return app_info


def app_key(provider_name, app_name):
    return provider_name + "." + app_name
