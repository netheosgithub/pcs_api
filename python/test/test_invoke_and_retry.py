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

from pcs_api.models import RetryStrategy, RequestInvoker
from pcs_api.cexceptions import CRetriableError, CHttpError

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.DEBUG)


def get_my_job(server_responses):
    get_my_job.invoke_count = 0
    def my_job(*args, **kwargs):
        logger.info('In my_job: arg1=%s, arg2=%s', args[0], args[1])
        response = server_responses[get_my_job.invoke_count]
        get_my_job.invoke_count += 1
        return response
    return my_job


def my_validate(response, c_path):
    logger.info('In my_validate: response= %s', response)
    status_code = int(response[0:3])
    reason = response[4:]
    if status_code == 200:
        return response
    if status_code == 400:
        # Such errors are not retriable, so we raise direct exception:
        raise CHttpError(request_method='GET',
                         request_path='/foo/bar',
                         status_code=status_code, reason=reason,
                         message='Looks like a bad request?')
    if status_code == 500:
        e=CHttpError(request_method='GET',
                     request_path='/foo/bar',
                     status_code=status_code, reason=reason,
                     message='Bad gateway')
        # Such requests should be retried, so we wrap them:
        raise CRetriableError(e)
    if status_code == 503:
        e = CHttpError(request_method='GET',
                       request_path='/foo/bar',
                       status_code=status_code, reason=reason)
        # Retry-After header may indicate a delay: forward it to CRetriableError
        retry_after=2
        raise CRetriableError(e, delay=retry_after)
    # Strange response:
    raise ValueError("Unexpected response: %s" % response)


def test_retry_and_invoker():
    server_responses = ['500 burp1', '503 burp2', '200 OK']
    ri = RequestInvoker()
    ri.validate_response = my_validate

    # If we try only once, it should fail with error 500:
    rs = RetryStrategy(nb_tries=1, first_sleep=1)
    ri.do_request = get_my_job(server_responses)
    with pytest.raises(CHttpError) as excinfo:
        rs.invoke_retry(ri.invoke, 1, 2)
    assert excinfo.type == CHttpError
    assert excinfo.value.status_code == 500

    # If we try twice, it should fail with error 503:
    rs = RetryStrategy(nb_tries=2, first_sleep=1)
    ri.do_request = get_my_job(server_responses)
    with pytest.raises(CHttpError) as excinfo:
        rs.invoke_retry(ri.invoke, 1, 2)
    assert excinfo.type == CHttpError
    assert excinfo.value.status_code == 503

    # If we try three times, it should work:
    rs = RetryStrategy(nb_tries=3, first_sleep=1)
    ri.do_request = get_my_job(server_responses)
    response = rs.invoke_retry(ri.invoke, 1, 2)
    assert response == '200 OK'


def test_retry_and_invoker2():
    server_responses = ['500 burp1', '400 KO']
    ri = RequestInvoker()
    ri.validate_response = my_validate

    # If we try only once, it should fail with error 500:
    rs = RetryStrategy(nb_tries=1, first_sleep=1)
    ri.do_request = get_my_job(server_responses)
    with pytest.raises(CHttpError) as excinfo:
        rs.invoke_retry(ri.invoke, 1, 2)
    assert excinfo.type == CHttpError
    assert excinfo.value.status_code == 500

    # If we try three times, it should fail with error 400
    # becuase this error is not retriable:
    rs = RetryStrategy(nb_tries=3, first_sleep=1)
    ri.do_request = get_my_job(server_responses)
    with pytest.raises(CHttpError) as excinfo:
        rs.invoke_retry(ri.invoke, 1, 2)
    assert excinfo.type == CHttpError
    assert excinfo.value.status_code == 400


