# -*- coding: utf-8 -*-

from __future__ import print_function
from setuptools import setup, find_packages
import io
import os
import re


def read(*filenames, **kwargs):
    encoding = kwargs.get('encoding', 'utf-8')
    sep = kwargs.get('sep', '\n')
    buf = []
    for filename in filenames:
        with io.open(filename, encoding=encoding) as f:
            buf.append(f.read())
    return sep.join(buf)

# Extract data from version file:
version_py = read('pcs_api/_version.py')
metadata = dict(re.findall("__([a-z]+)__ = '([^']+)'", version_py))
readme = 'readme_pypi.rst'
long_description = read(readme)

setup(
    name='pcs-api',
    version=metadata['version'],
    url='https://github.com/netheosgithub/pcs_api',
    download_url='https://github.com/netheosgithub/pcs_api/releases',
    license='Apache Software License V2.0',
    author='Netheos',
    tests_require=['pytest>=2.5.2'],
    install_requires=[
        'oauthlib>=0.6.2',
        'requests>=2.2.0',
        'requests-oauthlib>=0.4.0',
        'python-dateutil>=2.2'
    ],
    author_email='contact@netheos.net',
    description='A library that gives a uniform API to several personal cloud files storage providers (Dropbox, Google Drive...)',
    long_description=long_description,
    packages=find_packages(),
    include_package_data=True,
    platforms='any',
    classifiers=[
        'Programming Language :: Python',
        'Development Status :: 5 - Production/Stable',
        'Natural Language :: English',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2.7',
        'Topic :: Internet :: WWW/HTTP',
        'Topic :: Communications :: File Sharing',
        'Topic :: Software Development :: Libraries :: Python Modules',
    ]
)
