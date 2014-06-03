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

from .models import RetryStrategy

logger = logging.getLogger(__name__)


class StorageFacade(object):

    def __init__(self):
        self.providers_cls = {}

    @staticmethod
    def get_registered_providers():
        """Returns list of names of all registered providers."""
        return _instance.providers_cls.keys()

    def _register_provider(self, cls):
        name = cls.PROVIDER_NAME
        logger.info("Registering provider '%s' with class %r", name, cls)
        self.providers_cls[name] = cls

        return cls # For use as a decorator.

    @staticmethod
    def for_provider(provider_name):
        """Creates and returns a StorageBuilder object.

        :param provider_name: name of provider (always lower case)"""
        try:
            cls = _instance.providers_cls[provider_name]
            return StorageBuilder(provider_name, cls)
        except KeyError:
            raise ValueError('No provider implementation registered for name: %s' % provider_name)

_instance = StorageFacade()
register_provider = _instance._register_provider


class StorageBuilder(object):

    def __init__(self, provider_name, cls):
        self._provider_name = provider_name
        self._provider_cls = cls
        self.app_info_repos = None
        self.app_name = None
        self.user_creds_repos = None
        self.user_id = None
        self.for_bootstrapping = False
        self.retry_strategy = RetryStrategy(nb_tries=5, first_sleep=1)
        self.app_info = None
        self.user_credentials = None

    def app_info_repository(self, app_info_repos, app_name=None):
        self.app_info_repos = app_info_repos
        self.app_name = app_name
        return self

    def user_credentials_repository(self, user_creds_repos, user_id=None):
        self.user_creds_repos = user_creds_repos
        self.user_id = user_id
        return self

    def for_bootstrap(self):
        """OAuth bootstrap is not obvious: storage must be instantiated
        _without_ users credentials (for retrieving user_id thanks to access_token).
        As this use case is unlikely (instantiating a storage without user credentials),
        this method indicates the specificity: no 'missing users credentials' error
        will be raised."""
        self.for_bootstrapping = True
        return self

    def retry_strategy(self, retry_strat):
        """Set the retry requests strategy to be used by storage.
        If not set, a default retry strategy is used: retry 3 times
        with initial 1 second initial delay"""
        self.retry_strategy = retry_strat
        return self

    def build(self):
        """Construct a provider-specific storage implementation, by passing
        this builder in constructor. Each implementation gets its required
        informations from builder."""
        # Some generic checks:
        if self.app_info_repos is None:
            raise ValueError('Undefined application informations repository')
        if self.user_credentials_repository is None:
            raise ValueError('Undefined user credentials repository')
        self.app_info = self.app_info_repos.get(self._provider_name, self.app_name)

        if not self.for_bootstrapping:
            # Usual case: retrieve user credentials
            # Note that user_id may be undefined, repository should handle this:
            self.user_credentials = self.user_creds_repos.get(self.app_info, self.user_id)
        else:
            # Special case for getting initial oauth tokens:
            # we'll instantiate without any user_credentials
            pass
        return self._provider_cls(self)


class IStorageProvider(object):
    """Reference interface for storage providers.
    Inheritance is not necessary"""

    def provider_name(self):
        """Return name of provider for this storage"""
        raise NotImplementedError

    def get_user_id(self):
        """Return user identifier (login in case of login/password,
        or email in case of OAuth"""
        raise NotImplementedError

    def get_quota(self):
        """Returns a CQuota object."""
        raise NotImplementedError

    def list_root_folder(self):
        """Equivalent to list_folder(CPath('/'))"""
        raise NotImplementedError

    def list_folder(self, c_folder_or_c_path):
        """Return a map of files present in given CFolder, or at given CPath.
        keys of map are CPath objects, values are CFile objects (CFolder or CBlob).
        Return None if no folder exists at given path.
        Raise CInvalidFileTypeError if given path is a blob.
        Note: objects in returned map may have incomplete information."""
        raise NotImplementedError

    def create_folder(self, c_path):
        """Create a folder at given path, with intermediary folders if needed.
        Returns True if folder has been created, False if was already existing.
        Raise CInvalidFileType error if a blob exists at this path."""
        raise NotImplementedError

    def delete(self, c_path):
        """Delete blob, or recursively delete folder at given path.
        Returns True if at least one file was deleted, False if no object was existing at this path."""
        raise NotImplementedError

    def get_file(self, c_path):
        """Return detailed file informations at given path, or None
        if no object exists at this path"""
        raise NotImplementedError

    def download(self, download_request):
        """Download a blob from provider to a byte sink,
        as defined by the download_request object.
        Raise CFileNotFoundError if no blob exists at this path.
        Raise CInvalidFileTypeError if a folder exists at specified path."""
        raise NotImplementedError

    def upload(self, upload_request):
        """Upload a byte source to provider, as defined by upload_request object.
        If a blob already exists it is replaced.
        Raise CInvalidFileTypeError if a folder already exists at specified path."""
        raise NotImplementedError
