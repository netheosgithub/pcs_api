Getting started
===============

##Create a new project

Before creating any application, you may build the library. This process is described in this [page](build.md).

Otherwise, add a reference to pre-built releases in your project dependencies. Releases are available on maven central
for Java and Android, and pypi for Python.

You must rebuild yourself for C++: no prebuilt binaries are available.

###Python

Use of `pip` is recommended. Add the following dependency in `requirements.txt`:
```
pcs-api>=1.0
```

###Java

The simplest way to use the library is creating a maven (version 3+) project. Just add the dependency in the `pom.xml` file:
```xml
<dependency>
    <groupId>net.netheos</groupId>
    <artifactId>pcs-api-java</artifactId>
    <version>1.0.2</version>
</dependency>
```
If the application does not uses maven, the following jars must be linked:

* pcs-api-java-1.0.2.jar
* httpclient-4.2.6.jar
* httpcore-4.2.6.jar
* json-20131018.jar
* slf4j-api-1.7.5.jar
* jcl-over-slf4j-1.7.5.jar

###Android

Android Studio is needed to build an android application.
It can be downloaded [here](http://developer.android.com/sdk/installing/studio.html).
To use the library, just add the dependency in the `appname.gradle` file:
```gradle
compile 'net.netheos:pcs-api-android:1.0.2'
```

Note that Android platform already provides the http client and json jars. The platform agnostic logging API slf4j
is fullfilled by `slf4j-android`. The proper dependencies are declared in pcs-api-android pom.

###C++

Use `sample` cmake CMakeLists.txt file as a base for your project.
`libpcs_api` is built as a static library.


##Using the library

Once everything is setup (OAuth2 refresh token and credentials repositories, see hereafter),
instantiating a provider storage facade object is done with the following code:

In Python:
```python
   # Instantiate provider storage facade:
   storage = StorageFacade.for_provider('hubic') \
               .app_info_repository(app_info_repo) \
               .user_credentials_repository(user_credentials_repo) \
               .build()
```

In Java:
```java
   // Instantiate provider storage facade:
   IStorageProvider storage = StorageFacade.forProvider("hubic")
               .setAppInfoRepository(appInfoRepo)
               .setUserCredentialsRepository(userCredentialsRepo, null)
               .setHttpClient(customClient) // Optional: redefine the HttpClient
               .build();
```

For android, it is useful to define a custom HttpClient using
[AndroidHttpClient](http://developer.android.com/reference/android/net/http/AndroidHttpClient.html)
to be compliant with the platform.
The client to be used can be set by calling the `setHttpClient()` method, as decribed in the previous example).

In C++:
```C++
#include <memory>
#include "pcs_api/storage_facade.h"

std::shared_ptr<AppInfoRepository> app_info_repo = ...
std::shared_ptr<UserCredentialsRepository> user_credentials_repo = ...
std::shared_ptr<pcs_api::IStorageProvider> storage =
   StorageFacade::ForProvider("hubic")
               .app_info_repository(app_info_repo, "")
               .user_credentials_repository(user_credentials_repo, "")
               .Build();
```

pcs_api hides any cpprestsdk headers so you do not need any headers files for compiling.
However, for fine tuning (ie settings timeout or defining proxy), it is possible to include
some cpprestsdk headers, and then get access to `StorageBuilder::http_client_config()` for tweakings.
Refer to cpprestsdk documentation for details.

From there storage gives you access to remote files.
All remote paths are handled by `CPath` objects (a wrapper class containing a pathname of a remote file).
A CPath is always absolute, and uses slash separators between folders (anti-slashs are forbidden in CPath).

A remote folder is handled as a `CFolder` object, and a remote regular file is handled by a `CBlob` object.
Both classes inherit `CFile`.

In Python:
```python
   quota = storage.get_quota()
   print('User quota is:',quota)

   root_folder_content = storage.list_root_folder()
   # root_folder_content is a dict
   # keys are files paths, values are CFolder or CBlob objects for getting details (length, content-type...)
   print("Root folder content = ", root_folder_content)

   # Create a new folder:
   fpath = CPath('/new_folder')
   storage.create_folder(path)

   # Upload a local file in this folder:
   # In case intermediary folders are missing, they are created before upload
   bpath = fpath.add('new_file')
   upload_request = CUploadRequest(bpath, FileByteSource('my_file.txt')).content_type('text/plain')
   storage.upload(upload_request)

   # Download back the file:
   download_request = CDownloadRequest(bpath, FileByteSink('my_file_back.txt'))
   storage.download(download_request)

   # delete remote folder: (always recursive)
   storage.delete(fpath)
```
Refer to IStorageProvider interface docstrings for reference.

In Java:
```java
   CQuota quota = storage.getQuota();
   System.out.println("User quota is: " + quota);

   CFolderContent rootFolderContent = storage.listRootFolder();
   System.out.println("Root folder content = " + rootFolderContent);

   // Create a new folder:
   CPath fpath = new CPath("/new_folder");
   storage.createFolder(path);

   // Upload a local file in this folder:
   // In case intermediary folders are missing, they are created before upload
   CPath bpath = fpath.add("new_file");
   CUploadRequest uploadRequest = new CUploadRequest(bpath, new FileByteSource("my_file.txt"));
   uploadRequest.setContentType("text/plain");  // optional
   storage.upload(uploadRequest);

   // Download back the file:
   CDownloadRequest downloadRequest = new CDownloadRequest(bpath, new FileByteSink("my_file_back.txt"));
   storage.download(downloadRequest);

   // delete remote folder: (always recursive)
   storage.delete(fpath);
```

In C++:

Note that Windows platforms use wide strings (UTF-16), whereas other platforms use narrow strings (UTF-8).
The PCS_API_STRING_T macro permits to write portable code: it is OS-dependent and should be used for strings
definitions if program is aimed to be portable (see usage below).
This OS dependent string type is `pcs_api::string_t`.
```C++
   #include "pcs_api/model.h"
   #include "pcs_api/file_byte_source.h"
   #include "pcs_api/file_byte_sink.h"

   using namespace pcs_api;

   CQuota quota = storage->GetQuota();
   std::cout << "User quota is: " << quota << std::endl;

   std::shared_ptr<CFolderContent> root_folder_content = storage->ListRootFolder();
   std::cout << "Root folder content = " << root_folder_content;

   // Create a new folder:
   CPath fpath(PCS_API_STRING_T("/new_folder"));
   storage->CreateFolder(fpath);

   // Upload a local file in this folder:
   // In case intermediary folders are missing, they are created before upload
   CPath bpath = fpath.Add(PCS_API_STRING_T("new_file"));
   std::shared_ptr<ByteSource> fbs = std::make_shared<FileByteSource>(boost::filesystem::path("my_file.txt"));
   CUploadRequest upload_request(bpath, fbs);
   upload_request.set_content_type(PCS_API_STRING_T("text/plain"));  // optional
   storage->Upload(upload_request);

   // Download back the file:
   std::shared_ptr<ByteSink> bsink = std::make_shared<FileByteSink>(boost::filesystem::path("my_file_back.txt"));
   CDownloadRequest download_request(bpath, bsink);
   storage->Download(download_request);

   // delete remote folder: (always recursive)
   storage->Delete(fpath);
```

See `samples` directory and functional tests for other code examples (running these requires some setup though).

Listing a folder (or inquiring blob details) will return None (or null in Java, or empty smart pointer in C++)
if remote object does not exist.
Also deleting a non-existing file is not an error but merely returns false.
However exceptions are raised if trying to download a non-existing file,
or if creating a folder but a blob already exists at the same path.

A `Storage` object (`IStorageProvider` in C++) is thread safe, provided the different threads do not operate on the same objects.
Not all operations are atomic
(for example --depending on provider-- uploading a blob may result in intermediate folders creation. pcs_api
does this in two steps: check if folders exist, create them if not. A race condition exists if several threads
attempt to upload to the same non existing folder at the same time: duplicated folders may be created).

Now what are these objects required for building storage facade: app_info_repo, users_credentials_repo?
These objects are used to manage secrets: application secrets, and users secrets.

app_info_repo is an `AppInfoRepository` object (interface).
This object is read-only, it stores applications OAuth information: appId, appSecret, scope, redirect url, etc.
The single method of AppInfoRepository is:

In Python:
```python
get(provider_name, app_name=None)  # returns an AppInfo object.
```

In Java:
```java
AppInfo get(String providerName, String appName);  // returns an AppInfo object.
```

In C++:
```C++
// returns an AppInfo reference (owned by repository) / virtual method:
const AppInfo& GetAppInfo(const std::string& provider_name,
                          const std::string& app_name) const;
```

Note: if repository contains a single application for given provider,  the *app name* can be omitted
(or set to null in Java / empty string in C++).

user_credentials_repo is an `UserCredentialsRepository` object (interface).
This object is read/write : it stores users credentials (access_token, refresh_token, expiration date, etc.
or password for non OAuth providers) and persist new tokens when they are refreshed.
Methods are:

In python:
```python
get(app_info, user_id=None)  # returns a UserCredentials object
save(user_credentials)  # persists the given user_credentials object
```

In Java:
```java
UserCredentials<?> get(AppInfo appInfo, String userId);  // returns a UserCredentials object
void save(UserCredentials<?> userCredentials) throws IOException;  // persists the given user_credentials object
```

In C++ (methods are virtual):
```C++
// returns a UserCredentials object pointer, owned by client:
std::unique_ptr<UserCredentials> Get(const AppInfo& app_info, const std::string& user_id) const;
// persists the given user_credentials object:
void Save(const UserCredentials& user_credentials);
```

Note: if repository contains a single user for the given application, the *user id* may be omitted
(or set to null in Java / empty string in C++).

See [OAuth2](oauth2.md) page to better understand these objects.
