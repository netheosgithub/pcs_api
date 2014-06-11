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
import random
import string
import threading
import time
import datetime
import dateutil

from pcs_api.models import CPath

TEST_FOLDER_PREFIX = '/pcs_api_tmptest_'
# For checking modification times : how much difference do we allow ?
# TODO : one may use a NTP reference here, hoping providers are time synchronized
TIME_ALLOWED_DELTA_s = 120

logger = logging.getLogger(__name__)


def assert_datetime_is_almost_now(dtactual):
    dt_now = datetime.datetime.fromtimestamp(time.time(), dateutil.tz.tzutc())
    assert_datetimes_almost_equals(dt_now, dtactual)


def assert_datetimes_almost_equals(dtexp, dtactual):
    delta = dtexp - dtactual
    diff = abs(delta.total_seconds())
    if diff > TIME_ALLOWED_DELTA_s:
        logger.error('Datetimes are very different: exp=%s  actual=%s', dtexp, dtactual)
    assert diff <= TIME_ALLOWED_DELTA_s


def cleanup_test_folders(storage):
    """Cleanup any test folder that may still exist at the end of tests"""
    root_content = storage.list_root_folder()
    for path in root_content:
        if path.path_name().startswith(TEST_FOLDER_PREFIX):
            logger.info('Deleting old test folder: %s', path)
            storage.delete(path)


def generate_test_path(c_root_path=None):
    """Generate a temp folder for tests."""
    temp_pathname = ''.join(random.choice(string.ascii_uppercase + string.digits) for x in range(6))
    if c_root_path is None or c_root_path.is_root():
        temp_path = CPath(TEST_FOLDER_PREFIX + temp_pathname)
    else:
        temp_path = c_root_path.add(temp_pathname)
    return temp_path


def generate_random_bytearray(size, rnd=None):
    """Return a bytearray of random data with specified size.
    If seed is specified, it permits to generate pseudo-random data"""
    if not rnd:
        rnd = random.Random()
    def randbytes(n):
        for _ in xrange(n):
            yield rnd.getrandbits(8)
    return bytearray(randbytes(size))


def test_in_threads(nb_threads, threads_base_name, func, *args, **kwargs):
    threads = []
    for i in range(nb_threads):
        t = TestThread("%s_%d" % (threads_base_name, i), func, *args, **kwargs)
        t.start()
        threads.append(t)

    # Now wait for threads end (handle Ctrl^C):
    to_be_joined = list(threads)
    while len(to_be_joined) > 0:
        to_be_joined = [t.join(1) for t in threads if t.isAlive()]

    # Show what happened:
    for t in threads:
        if t.catched_exception:
            logger.error('Thread %r failed with exception: %s', t, t.catched_exception)
        elif not t.success:
            logger.error('Thread %r failed !?', t)  # Should not occur: defensive code
        else:
            logger.info('Thread %r finished successfully', t)
    assert all(t.success for t in threads)


class TestThread(threading.Thread):
    """Utility class to run the same single test by multiple threads"""
    def __init__(self, name, test_func, *args, **kwargs):
        super(TestThread, self).__init__(name=name)
        self._test_func = test_func
        self.args = args
        self.kwargs = kwargs
        self.success = False
        self.daemon = True
        self.catched_exception = None

    def run(self):
        try:
            self._test_func(*self.args, **self.kwargs)
            self.success = True
        except Exception as e:
            logger.exception('Thread failed: %r', self)
            self.catched_exception = e

