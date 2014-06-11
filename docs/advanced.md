Advanced
========

### Exceptions

The syntax are different depending of the language. In Python the syntax `error` is used and in Java it is `Exception`.
In the following lines the Python syntax will be used, but the descriptions are identical in Java (replace `Error` by `Exception`)

All exceptions are CStorageError objects, with sub-classes :
- CInvalidFileTypeError : conflict folder/blob (example : trying to download a folder)
- CHttpError : generic server http error (raw exception, not abstracted from provider)
- CFileNotFoundError : a requetsed file do not exists on the server
- CAuthenticationError : a specific CHttpError when status code is 401

Care is taken to extract provider error messages in order to build nice CStorageError exceptions.

### Logging

In Python, logging is performed by the standard `logging` package. Logging messages at a level lower than INFO
will leak confidential information (either by pcs_api itself, or by dependencies : requests, requests-oauthlib, oauthlib, urllib3, httplib...).

In Java, logging is performed by the `slf4j` library.
On android a specific implementation of this library is used to bypass `Logcat`. The logging level works like in Python.


### Retry strategy

Cloud storage providers sometimes encounter technical problems and return http 5xx errors.
By default, pcs_api will retry requests 5 times before giving up, with exponential back-off algorithm.

Other strategies may be used by supplying a RetryStrategy object when instantiating storage.
