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
import datetime
import dateutil.tz

from pcs_api.providers import swift


def test_parse_x_timestamp():
    # Check we won't break if a header is missing:
    dt = swift.parse_x_timestamp({})
    assert dt is None

    headers = {'X-Timestamp': '1383925113.43900'}  # 2013-11-08T16:38:33.439+01:00
    dt = swift.parse_x_timestamp(headers)
    assert dt == datetime.datetime(2013, 11, 8, 15, 38, 33, 439000, tzinfo=dateutil.tz.tzutc())

    headers = {'X-Timestamp': '138_burp_3925113.43900'}
    dt = swift.parse_x_timestamp(headers)
    assert dt is None


def test_parse_last_modified():
    # Check we won't break if a header is missing:
    json = {}
    dt = swift.parse_last_modified(json)
    assert dt is None

    json = {'last_modified': '2014-02-12T16:13:49.346540'}  # this is UTC
    dt = swift.parse_last_modified(json)
    assert dt == datetime.datetime(2014, 2, 12, 16, 13, 49, 346540, tzinfo=dateutil.tz.tzutc())


def test_sort_json():
    to_sort = [
        {u'name': u'toto8/bar/a_file'},
        {u'name': u'toto8/toto9'},
        {u'name': u'toto8/xxx/'},
        {u'name': u'toto8/xxx/another_file'},
        {u'name': u'toto8/toto9/toto10'},
        {u'name': u'toto8/toto9/toto10/toto11/BURP'},
        {u'name': u'toto8/toto9/toto10/toto11'},
        {u'name': u'toto8/toto9/toto10/toto11/BURP/pep8.tar.gz'}
    ]
    s = sorted(to_sort, key=lambda s: s['name'], reverse=True)
    print(s)
