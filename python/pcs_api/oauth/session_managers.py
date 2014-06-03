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
import time
import urlparse
import requests
from oauthlib.oauth2 import TokenExpiredError
from requests_oauthlib import OAuth2Session
import logging
import threading

from ..credentials.app_info import AppInfo
from ..credentials.user_credentials import UserCredentials
from ..cexceptions import CStorageError

logger = logging.getLogger(__name__)


class AbstractSessionManager(object):

    def __init__(self, user_credentials):
        self._user_credentials = user_credentials

    def get_session(self):
        """Return a requests session-like object suitable to issue authenticated http requests to provider"""
        raise NotImplementedError


class BasicAuthSessionManager(AbstractSessionManager):
    """Add http basic authentication header: yandex, ...

    Note: this is actually NOT an oauth manager !"""

    def __init__(self, user_credentials):
        super(BasicAuthSessionManager, self).__init__(user_credentials)
        # Some checks:
        if user_credentials.user_id is None:
            raise ValueError("Undefined user_id in user_credentials")
        creds = self._user_credentials.credentials();
        if not 'password' in creds:
            raise ValueError("User credentials do not contain user password")

    def get_session(self):
        session = requests.Session()
        session.auth = requests.auth.HTTPBasicAuth(self._user_credentials.user_id,
                                                   self._user_credentials.credentials().get('password'))
        return session


class DigestAuthSessionManager(AbstractSessionManager):
    """Handle http digest authentication: CloudMe, ..."""

    def __init__(self, user_credentials):
        super(DigestAuthSessionManager, self).__init__(user_credentials)
        # Some checks:
        if user_credentials.user_id is None:
            raise ValueError("Undefined user_id in user_credentials")
        creds = self._user_credentials.credentials();
        if not 'password' in creds:
            raise ValueError("User credentials do not contain user password")

        # HTTPDigestAuth objects: in order to avoid double requests each time,
        # such objects must survive between requests.
        # However they can not be shared between threads, so we keep a cache of them for each thread:
        self._digests_auth = threading.local()

    def get_session(self):
        session = requests.Session()
        try:
            digest_auth = self._digests_auth.digest_auth
        except:
            digest_auth = requests.auth.HTTPDigestAuth(self._user_credentials.user_id,
                                                       self._user_credentials.credentials().get('password'))
            self._digests_auth.digest_auth = digest_auth

        session.auth = digest_auth
        return session


class OAuth2SessionManager(AbstractSessionManager):
    """OAuth2 authorization manager (used by many providers)
    """
    def __init__(self, oauth2_provider_params, app_info,
                 user_credentials_repository=None,
                 user_credentials=None):
        super(OAuth2SessionManager, self).__init__(user_credentials)
        self._oauth2_provider_params = oauth2_provider_params
        self._app_info = app_info
        self._user_credentials_repository = user_credentials_repository
        self._refresh_lock = threading.RLock()
        # Some checks if we already have user_credentials:
        if user_credentials is not None:
            creds = self._user_credentials.credentials()
            if not 'access_token' in creds:
                raise ValueError("User credentials do not contain any access token")

    def get_authorize_url(self):
        oauth = OAuth2Session(client_id=self._app_info.app_id,
                              redirect_uri=self._app_info.redirect_url,
                              scope=self._oauth2_provider_params.scope_for_authorization(self._app_info.scope))
        url, state = oauth.authorization_url(self._oauth2_provider_params.authorize_url)
        return url, state

    def fetch_user_credentials(self, code_or_url, state):
        """This is for bootstrapping Oauth2 and getting an initial refresh token."""
        oauth = OAuth2Session(client_id=self._app_info.app_id,
                              redirect_uri=self._app_info.redirect_url,
                              scope=self._oauth2_provider_params.scope_for_authorization(self._app_info.scope),
                              state=state)
        if code_or_url.startswith('http://') or code_or_url.startswith('https://'):
            # It is an URL:
            url = code_or_url
            code = None
            # URL may contain granted scope:
            query = urlparse.urlparse(url).query
            params = dict(urlparse.parse_qsl(query))
            granted_scope_str = params.get('scope', None)
            if granted_scope_str is not None:
                #logger.debug("granted scope str= %s", granted_scope_str)
                granted_scope = self._oauth2_provider_params.granted_scope(granted_scope_str)
                logger.debug("granted scope = %s", granted_scope)
        else:
            # It is a code:
            url = None
            code = code_or_url
        token = oauth.fetch_token(self._oauth2_provider_params.access_token_url,
                                  authorization_response=url,
                                  code=code,
                                  client_secret=self._app_info.app_secret)
        if self._user_credentials is None:
            self._user_credentials = UserCredentials(self._app_info, None, token)
        else:
            self._user_credentials.set_new_credentials(token)
        return self._user_credentials

    def do_request(self, *args, **kwargs):
        already_refreshed_token = False
        while True:
            # We always take a new session: required to get fresh access_token
            session = OAuth2Session(client_id=self._app_info.app_id,
                                    token=self._user_credentials.credentials())
            try:
                return session.request(*args, **kwargs)
            except TokenExpiredError as tee:
                # If we didn't try already, get a new access_token, we'll refresh it.
                # this may be a no-op if another thread has just done the same thing:
                if not already_refreshed_token:
                    logger.debug('Expired access_token: will refresh')
                    self.refresh_token()
                    already_refreshed_token = True
                    # And we'll request again
                else:
                    # We have refreshed already: this is strange
                    raise CStorageError('Expired token after refresh ? Giving up', tee)

    def refresh_token(self):
        """Access tokens are refreshed after expiration (before sending request).
        This method refreshes token from the given session and stores new token
        in this session manager object.
        Method is synchronized so that no two threads will attempt to refresh
        at the same time. If a locked thread sees that token has already been
        refreshed, no refresh is attempted either.

        Not all providers support tokens refresh (ex: Dropbox)."""
        if not self._oauth2_provider_params.refresh_token_url:
            # Provider does not support token refresh: we are dead
            raise CStorageError('Invalid or expired token ; provider does not support token refresh')

        current_creds = self._user_credentials.credentials()
        with self._refresh_lock:
            after_lock_creds = self._user_credentials.credentials()
            if after_lock_creds == current_creds:
                logger.debug('This thread will actually refresh token: %r', threading.current_thread())
                session = OAuth2Session(client_id=self._app_info.app_id,
                                        token=self._user_credentials.credentials())
                extra = {'client_id': self._app_info.app_id,
                         'client_secret': self._app_info.app_secret}
                new_token = session.refresh_token(self._oauth2_provider_params.refresh_token_url,
                                                  **extra)
                self._token_saver(new_token)
            else:
                logger.debug('Not refreshed token in this thread, already done')

    def _token_saver(self, new_token):
        """callback of requests-oauthlib: called when token has been refreshed.

        In case no refresh_token has been given by provider, the old one has been kept
        by the framework so this method only needs to update and persist given token.

        :param new_token: json dictionary containing access_token, etc."""
        logger.debug("Will persist refreshed token: %s", new_token)
        if 'expires_in' in new_token:
            # If token contains expiration time,
            # convert relative time to absolute timestamp.
            # Used by oauthlib when token will be read again in the future
            new_token['expires_at'] = time.time() + int(new_token['expires_in'])
        # Update current user credentials:
        self._user_credentials.set_new_credentials(new_token)
        # And save this information:
        self._user_credentials_repository.save(self._user_credentials)

