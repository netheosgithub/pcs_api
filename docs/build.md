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

C++
---

Builds and tests have been performed with MSVC 2013 (aka Visual C++ 12.0) and gcc 4.8.2.
Any compiler with C++11 support should be supported.
Other required tools and packages:
- `CMake` V2.8
- cpprestsdk V2.4 (as a NuGet package for Windows; higher versions are likely to be supported)
- libboost 1.55 (http://www.boost.org/users/history/version_1_55_0.html).
  Boost prebuilt Windows binaries are recommended, as CMake_ext_deps.inc file expects this folders hierarchy.
- Git for Windows (http://git-scm.com/download/win)

Once these tools and dependencies have been installed, follow these steps for

##### Windows configuration:

1. Open git console and clone repository, then cd in your repository.
2. Also clone third party sub modules (only gtest for now): `git submodule update --init --recursive`
3. In bash console, cd to `cpp/` subfolder
4. Edit `CMake_ext_deps.inc` to define boost location.
   If prebuilt binaries have been installed, it should be sufficient to change line:
    `set( BOOST_ROOT  "c:/Users/pcsapi/pcs_api/boost_1_55_0" )`
   (this definition is for cmake, so use standard windows pathnames;
   Windows accepts slashs instead of anti-slashs).
   If you have a custom boost installation, it may not be required to set BOOST_LIBRARYDIR variable.
5. If `cmake.exe` is not in your PATH, you can edit `run_cmake_windows.sh` to configure cmake.exe absolute path.
6. Run script `./run_cmake_windows.sh` to create MSVC 2013 projects files.
   One build folder is created for 32 bits architecture (`build_Win32`),
   another one is created for 64 bits (`build_x64`).
   So `run_cmake_windows.sh` actually generates two build dirs, with two sets of MSVC projects files.
   (as an alternative, run cmake GUI, but keep build folder names: build_Win32, build_x64).

   If all paths have been setup correctly, cmake should work fine and MSVC projects and solution
   files are created.

##### Windows build:

1. Launch MSVC, and open solution file in build_xxx folder (either 32 or 64 bits): pcs_api.sln
2. Install CPP ReST SDK dependency into build folder:
   select menu tools/Library package manager/Package manager console, then enter command in console:
    `Install-Package cpprestsdk -Version 2.4`.
   This downloads and installs NuGet headers and binary package into *{build_folder}*/packages/cpprestsdk.2.4.0
   (so this download is required for both architectures).
   Dependent nuGET packages are also downloaded: openssl, zlib
3. By now it is possible to build all projects: gtest, libpcs_api, pcs_api_test,
   and sample : menu Build/Build solution.
   This creates:
    - a static library: *{build_folder}*/libpcs_api/Debug/pcs_api.lib
    - a test executable: *{build_folder}*/libpcs_api/test/Debug/pcs_api_test.exe
    - a sample executable: *{build_folder}*/sample/Debug/sample.exe

##### Windows run (sample and tests):

For launching binaries, all dependent DLL must be present in PATH variable,
so configuring PATH can be tedious.
The bash script file `run_exe_windows.sh` does this job:

- it can configure PATH before running an executable, depending on architecture
  and configuration, or
- it can copy all required DLL in the same folder as executable (option `-C`),
  so that no PATH configuration is required for running (very useful
  for debugging from MSVC ; env. variable PCS_API_REPOSITORY_DIR must be defined).
  Run this script without arguments to see all options.

Examples: in order to run unit and functional tests for 64 bits/Debug version:
  `run_exe_windows.sh pcs_api_test other options here...`

In order to run sample for 32 bits/Release version (-a is for architecture, -c is configuration)
  `run_exe_windows.sh -a Win32 -c Release sample other options here...`

This binaries can be run only after applications have been registered provider side.
Tests can be launched only after a test user account has been created, and user has
authorized application to access his files.
Date and time must be properly set on the local machine; some tests are disabled/ignored,
depending on cloud storage provider and operating system.

The PCS_API_REPOSITORY_DIR environment variable is set to *{git_root_folder}*/repositories

##### Linux configuration:

1. cpprestsdk must be cloned and built: refer to specific doc
   (https://casablanca.codeplex.com/wikipage?title=Setup%20and%20Build%20on%20Linux).
   Set working copy to tag v2.4.0 before building: `git checkout v2.4.0` and keep the
   `build.release` folder name, as it is referenced from CMakeLists.txt
   You may need to edit the `Release/src/CMakeLists.txt` file and remove the `-Werror` from `CMAKE_CXX_FLAGS`.
2. Clone pcs_api repository and sub modules as explained above for Windows.
3. Create a symbolic link `cpprest` in cpp/ that refers to *{cpprest_git_root_folder}*/Release folder.
   This link is referenced from CMake_ext_deps.inc
4. Edit `CMake_ext_deps.inc` to define boost location (if not installed in your OS).
5. Create a build folder `build_Release` and cd into it.
6. Run cmake command: `cmake .. -DCMAKE_BUILD_TYPE=Release`

   If all paths have been setup correctly, cmake should work fine and Makefiles are created.

##### Linux build:

1. In the build folder, type `make` to build all projects: gtest, libpcs_api, pcs_api_test,
   and sample : menu Build/Build solution.
   This creates:
    - a static library: *{build_folder}*/libpcs_api/libpcs_api.a
    - a test executable: *{build_folder}*/libpcs_api/test/pcs_api_test
    - a sample executable: *{build_folder}*/sample/sample

##### Linux run (sample and tests):

See Windows note above: these binaries can be run only after applications and user accounts
have been registered provider side, etc. The environment variable PCS_API_REPOSITORY_DIR
defines path of folder containing application info and user credentials files.
If not defined, program uses ../../repositories (ie the repositories folder at the root
of your git repository) so that it works out of box if files are present and programs are
launched from the build folder.

```shell
$ export PCS_API_REPOSITORY_DIR=/path/to/your/repositories/folder
$ ./libpcs_api/test/pcs_api_test other options here...
$ ./sample/sample provider_name other options here...
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

C++ sample
----------

This sample is automatically built after libpcs_api (see above). It is located in cpp/sample
instead of samples/cpp
