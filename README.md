pcs_api
=======

Personal Cloud Storages API: a library that abstract providers and gives a uniform API to several personal cloud storages providers.
As of today, the following providers are supported:
[Dropbox](https://www.dropbox.com)
[hubiC](https://hubic.com)
[Google Drive](http://www.google.com/drive/)
[CloudMe](https://www.cloudme.com)

API provides the following methods:

- list/create/delete folder
- upload/download/delete blob (a blob is a regular file)
- inquire about user and about used/available storage quota.

pcs_api is released under Apache License V2.0


Languages
---------

The API has been developped in many languages (Java, android and Python). Some samples are availables in the `samples` directory for each one.

### Java and android
An implementation exists for Java (1.6+). It is also compliant with android API 8+.
The base library uses a maven artefact with the following dependencies:

- *httpclient* for the network access
- *slf4j* for the logging
- *json* to parse the JSON server responses

The unit tests are done using the *testng* library.

[Android Studio](http://developer.android.com/sdk/installing/studio.html) has been used to build the android library. It generates an archives `aar` which can be used in an application. This works as a Maven artefact.

**WARNING:** the *aar* file can only be used with *Android Studio* ; it is not compatible with *Eclipse*.

### Python
An implementation exists for Python (tested on version 2.7).

Python implementation has some dependencies, as specified in [requirements.txt](python/requirements.txt) (*pip* and *virtualenv* are recommended for installation).
Basically, *requests* and *requests-oauthlib* are used for http requests and OAuth authentication, *pytest* is used for testing.


Getting started
---------------
See this [page](docs/getting_started.md).

Build
-----
See this [page](docs/build.md)

Features
--------
pcsapi can handle several authentications types:

- [OAuth2](docs/oauth2.md) (used by Dropbox, hubiC, Google Drive)
- Login / password (CloudMe uses http digest authentication)

Each provider has its specificity that are described [here](docs/provider_specifics.md);

The stream on files are wrapped by the same objects whatever the choosen provider. To read a file from the server the ByteSink interface must be used and to write a file, the ByteSource must be used.
More informations are available [here](docs/byte_sources_and_sinks.md);

For more technical informations, please read this [page](docs/advanced.md).
