Getting started
===============

##Create an application

Before creating any application, you must build the library. This process is described in this [page](build.md);

###Java

The better way to use the library is creating a maven (version 3+) project. Just add the dependency in the `pom.xml` file:
```xml
<dependency>
    <groupId>net.netheos</groupId>
    <artifactId>pcsapi</artifactId>
    <version>1.0-SNAPSHOT</version>
</dependency>
```
If the application do not uses maven, the following jars must be linked:

* pcsapi-1.0-SNAPSHOT.jar
* httpclient-4.2.6.jar
* httpcore-4.2.6.jar
* json-20131018.jar
* slf4j-api-1.7.5.jar
* jcl-over-slf4j-1.7.5.jar

###Android

Android Studio is needed to build an android application. It can be downloaded [here](http://developer.android.com/sdk/installing/studio.html). To use the library, just add the dependency in the `appname.gradle` file:
```gradle
compile 'net.netheos:pcsapi-android:1.0-SNAPSHOT'
```


##Using the library

Once everything is setup (OAuth2 refresh token and credentials repositories, see hereafter), instantiating a provider storage facade object is done with the following code:

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

For android, it is useful to define a custom HttpClient using [AndroidHttpClient](http://developer.android.com/reference/android/net/http/AndroidHttpClient.html) to be compliant with the platform. It can be setted by calling the method setHttpClient as decribed in the previous example).

From there storage gives you access to remote files. All remote paths are handled by CPath object. Folders separator is always / (anti-slashs are forbidden in CPath).

A remote folder is handled as a CFolder object, and a remote regular file is handled by a CBlob object. Both classes inherit CFile.

In Python
```python
   quota = storage.get_quota()
   print('User quota is:',quota)

   root_folder_content = storage.list_root_folder()
   # root_folder_content is a dict
   # keys are files paths, values are CFolder or CBlob providing details (length, content-type...)
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

   # delete remote folder:
   storage.delete(fpath)
```
Refer to IStorageProvider interface docstrings for reference.

In Java
```java
   CQuota quota = storage.getQuota();
   System.out.println("User quota is: " + quota);

   CFolderContent rootFolderContent = storage.listRootFolder();
   System.out.println("Root folder content = " + root_folder_content);

   // Create a new folder:
   CPath fpath = new CPath("/new_folder");
   storage.createFolder(path);

   // Upload a local file in this folder:
   // In case intermediary folders are missing, they are created before upload
   CPath bpath = fpath.add("new_file");
   CUploadRequest uploadRequest = new CUploadRequest(bpath, new FileByteSource("my_file.txt"));
   uploadRequest.setContentType("text/plain");
   storage.upload(uploadRequest);

   // Download back the file:
   CDownloadRequest downloadRequest = new CDownloadRequest(bpath, new FileByteSink("my_file_back.txt"));
   storage.download(downloadRequest);

   // delete remote folder:
   storage.delete(fpath);
```

See `samples` directory and functional tests for other code examples (running these requires some setup though).

Raw storage object is thread safe.

Listing a folder (or inquiring blob details) will return None (or null) if remote object does not exist. Also deleting a non-existing file is not an error but merely returns false. However exceptions are raised if trying to download a non-existing file, or if creating a folder but a blob already exists at the same path.

Now what are these objects required for building storage facade: app_info_repo, users_credentials_repo?
These objects are used to manage secrets : application secrets, and users secrets.

app_info_repo is an AppInfoRepository object (interface).
This object is read-only, it stores applications information: appId, appSecret, scope, redirect url, etc.
The single method of AppInfoRepository is:

In Python
```python
get(provider_name, app_name=None)  # returns an AppInfo object.
```

In Java
```java
AppInfo get(String providerName, String appName);  // returns an AppInfo object.
```

Note: if repository contains a single application for given provider,  the *app name* can be omitted.

user_credentials_repo is an UserCredentialsRepository object (interface).
This object is read/write : it stores users credentials (access_token, refresh_token, expiration date, etc.)
and persist new tokens when they are refreshed.
Methods are:

In python
```python
get(app_info, user_id=None)  # returns a UserCredentials object
save(user_credentials)  # persists the given user_credentials object
```

In Java
```java
UserCredentials<?> get(AppInfo appInfo, String userId);  // returns a UserCredentials object
void save(UserCredentials<?> userCredentials) throws IOException;  // persists the given user_credentials object
```

Note: if repository contains a single user for given application, the *user id* may be omitted.

See [OAuth2](oauth2.md) page to better understand these objects. 
