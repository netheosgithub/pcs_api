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
import os
import threading

from .user_credentials import UserCredentials


class UserCredentialsFileRepository:
    """This class is a simple reader/writer of users credentialsapplications informations
    from a plain text file with format:
    provider1.app_name_1.user_id1 = json_object
    provider1.app_name_1.user_id2 = json_object

    Sample code only: data is NOT encrypted in file, and this class does NOT scale if many users.
    This class is thread-safe, but NOT multi-processes safe.
    """

    def __init__(self, data_filename):
        self._data_filename = data_filename
        self._user_creds = {}
        self._lock = threading.Lock()
        # If a file already exists, read all users into our dict:
        if os.path.exists(self._data_filename):
            self._read_file()

    def get(self, app_info, user_id=None):
        """Retrieve user credentials for the given application and optional user id.
        If repository contains only one user credential for the given application,
        user_id may be left unspecified."""
        with self._lock:
            if user_id is not None:
                key = user_key(app_info, user_id)
                json_creds = self._user_creds[key]
            else:
                # loop over all credentials
                # cpu hungry but remember: this class is only for quick tests
                json_creds = None
                prefix = app_prefix(app_info)
                for k in self._user_creds:
                    if k.startswith(prefix):
                        user_id = k[len(prefix):]
                        if json_creds is None:
                            json_creds = self._user_creds[k]
                        else:
                            raise ValueError('Several user credentials found for application %s' % app_info)
                if json_creds is None:
                    raise ValueError('No user credentials found for application %s' % app_info)

            return UserCredentials(app_info, user_id, json_creds)

    def save(self, user_credentials):
        with self._lock:
            key = user_key(user_credentials.app_info, user_credentials.user_id)
            # in case this entry does not exist yet:
            self._user_creds[key] = user_credentials.credentials()
            # Write to file:
            self._write_file()

    def _read_file(self):
        with open(self._data_filename, "r") as data_file:
            for line in data_file:
                line = line.strip()
                if not line or line[0] == '#':  # ignore empty or comment lines
                    continue
                # split at first = sign ; left is key, right is credentials (login+password or oauth2 tokens)
                key_creds = line.split('=', 1)
                if len(key_creds) != 2:
                    raise ValueError("Not parsable line: "+line)
                key = key_creds[0].strip()
                json_creds = key_creds[1].strip()
                #print "key=",key
                #print "creds=",json_creds
                self._user_creds[key] = json.loads(json_creds)

    def _write_file(self):
        # We write to a temporary file first so that nothing is lost in case of failure:
        tmp_name = self._data_filename + '.tmp'
        with open(tmp_name, 'w') as data_file:
            data_file.write('# Lines format is key = value\n')
            data_file.write('# key is composed of providerName.appName.userId\n')
            data_file.write('# value is a json object containing tokens for this (user, application) couple.\n')
            data_file.write('# Note that token content is provider dependent.\n')
            data_file.write('# do NOT modify this file by hand: your modifications would be erased by next write.\n')
            for key, credentials in self._user_creds.iteritems():
                #print 'credentials = ',credentials
                value = json.dumps(credentials)
                line = key + ' = ' + value
                data_file.write(line+'\n')
        # Now we rename new file to old:
        try:
            os.rename(tmp_name, self._data_filename)
        except OSError:
            # happens on Windows as dest file already exists
            os.remove(self._data_filename)
            os.rename(tmp_name, self._data_filename)


def app_prefix(app_info):
    # Note: scope is ignored in key
    return '%s.%s.' % (app_info.provider_name, app_info.app_name)


def user_key(app_info, user_id):
    if user_id is None:
        raise ValueError("Undefined user_id")
    return '%s%s' % (app_prefix(app_info), user_id)

