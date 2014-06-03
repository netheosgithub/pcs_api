Build
=====

This page will describe the way to build pcsapi

Java
----
The Java build uses Maven (tested with version 3.2.1) and Java 6+. It can be downloaded [here](http://maven.apache.org/download.cgi). The Maven system integration is described on the download page. For pcs_api, no particular configuration is needed (only uses [Maven central](http://mvnrepository.com/) to get the dependencies).

To build the base project, just follow these steps:

1. Open a shell and set the current directory to `java`
```bash
cd java
```

2. Launch the maven compilation. This call will also starts the unit tests
```bash
mvn clean install
```

The compilation will generates the file `pcsapi-xxx.jar`.
The artifact is saved in the local maven repository (~/.m2/repository/).

Android
-------
The android build is based on the Java library. Here the [gradle](http://www.gradle.org/) framework is used. No third party application is needed for build because the android project already embeds gradle wrapper `gradlew`.

To build the project, just follow these steps:

1. Open a shell and set the current directory to `android`
```bash
cd android
```

2. Launch gradle compilation. This will not start the unit tests but create an `apk` file containing the unit test application (see the next chapter).
```bash
./gradlew clean assemble assembleDebugTest uploadArchives
```

The compilation will generates the file `pcsapi-android-xxx.aar`. This file can be used in any Android Studio project.
The artifact is saved in the local maven repository.

###Unit tests

These tests are identical to the Java tests.

To launch the unit tests, an emulator must be started, then the following commands must be executed:
```bash
export ANDROID_HOME=<sdk.path>
$ANDROID_HOME/platform-tools/adb push pcsapi-android/build/apk/pcsapi-android-debug-test-unaligned.apk /data/local/tmp/net.netheos.pcsapi.test
$ANDROID_HOME/platform-tools/adb shell pm install -r "/data/local/tmp/net.netheos.pcsapi.test"
$ANDROID_HOME/platform-tools/adb shell am instrument -w -r net.netheos.pcsapi.test/android.test.InstrumentationTestRunner
```
