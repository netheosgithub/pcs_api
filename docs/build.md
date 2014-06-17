Build
=====

This page describes how to build pcs_api and sample applications.

Java
----

The Java build uses Maven (tested with version 3.2.1) and Java 6+.
Maven can be downloaded [here](http://maven.apache.org/download.cgi).
The Maven system integration is described on the download page.
For pcs_api, no particular configuration is needed (only uses [Maven central](http://mvnrepository.com/) to get the dependencies).

To build the base project, just follow these steps:

1. Open a shell and set the current directory to `java`
```bash
cd java
```

2. Launch the maven compilation. This call will also starts the unit tests
```bash
mvn clean install
```

The compilation will generates the file `pcs-api-java-xxx.jar`.
The artifact is saved in the local maven repository (~/.m2/repository/).

Android
-------
The android build is based on the Java library. Here the [gradle](http://www.gradle.org/) framework is used.
No third party application is needed for build because the android project already embeds gradle wrapper `gradlew`.

To build the project, just follow these steps:

1. Open a shell and set the current directory to `android`
```bash
cd android
```

2. Launch gradle compilation. This will not start the unit tests but create an `apk` file containing the unit test application (see the next chapter).
```bash
./gradlew clean assemble assembleDebugTest uploadArchives
```

The compilation will generates the file `pcs-api-android-xxx.aar`. This file can be used in any Android Studio project.
The artifact is saved in the local maven repository.

###Unit tests

These tests are identical to the Java tests.

To launch the unit tests, an emulator must be started, then the following commands must be executed:
```bash
export ANDROID_HOME=<sdk.path>
$ANDROID_HOME/platform-tools/adb push pcs-api-android/build/apk/pcs-api-android-debug-test-unaligned.apk /data/local/tmp/pcs-api-android-debug-test-unaligned.tmp
$ANDROID_HOME/platform-tools/adb shell pm install -r /data/local/tmp/pcs-api-android-debug-test-unaligned.tmp
$ANDROID_HOME/platform-tools/adb shell am instrument -w -r net.netheos.pcsapi.test/android.test.InstrumentationTestRunner
```

Samples builds
==============

Samples exist solely for showing library usage, but use non-production implementations (AppInfoFileRepository
and UserCredentialsFileRepository).
Running samples requires registrating an OAuth2 application for each provider (exception for login/password
providers such as CloudMe), and also a user account.

Java sample
-----------

Java sample performs OAuth2 authorization workflow, list all files recursively, then upload and download a test file.

The pcs_api library must be built for java before this step.

Open a shell and set the current directory to `samples/java` and build using maven:
```bash
cd samples/java
mvn clean package
```

Location of credentials repository folder files is defined by an environment variable
(refer to code for actual values). Running sample can be achieved with maven:

```bash
export PCS_API_REPOSITORY_DIR=/path/to/your/repositories/folder
mvn exec:java
```

Android sample
--------------

The pcs_api library must be built for android before this step.

The android sample app performs OAuth2 authorization workflow thanks to an embedded web view,
then lists all files recursively. Only OAuth2 providers are supported for now.

Open a shell and set the current directory to `samples/android` and build using gradle:
```bash
cd samples/android
./gradlew clean build
```
This generates the apk:
```bash
$ ls -l sample/build/apk/
total 236
-rw-r--r-- 1  149027 jun  11 14:28 sample-debug-unaligned.apk
-rw-r--r-- 1  146028 jun  11 14:28 sample-release-unsigned.apk
```

AppInfoFileRepository file must be located on the SD-card, in `pcs_api` folder.
UserCredentialsRepository file is created into application private folder.

Note that OAuth2 authorization needs to be done only once, then tokens are persisted.
Replaying the authorization workflow requires the reinstallation of the sample application.

