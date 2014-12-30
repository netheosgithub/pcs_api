#!/bin/bash -e
#
# run_windows_exe.sh [-n pcs_api_test|sample] [-c Debug|Release] [-a Win32|x64]

# Default values:
CONFIG=Debug
ARCH=x64
TOOLS=v120

usage() {
    cat << EOF
usage: $0 [options] EXE_NAME [executable options...]

This script configures path and current folder to run an executable.
It can also copy dependent DLL in executable folder for simpler launching.

OPTIONS:
   -h   show this message
   -c   Configuration (Release or Debug ; defaults to $CONFIG)
   -a   Architecture (Win32 or x64 ; default to $ARCH)
   -C   Do not launch, only copy all dll next to executable: helps debugging from within MSVC

EXE_NAME can be 'pcs_api_test' to run tests, or 'sample'
All arguments after EXE_NAME are forwarded as EXE_NAME arguments.
EOF
exit $1
}
while getopts "hc:a:C" OPTION
do
    case $OPTION in
        h)
            usage 0
            ;;
        c)
            CONFIG="$OPTARG"
            ;;
        a)
            ARCH="$OPTARG"
            ;;
        C)
            COPY_DLL=1
            ;;
        ?)
            echo "Invalid option: $OPTION"
            usage 1
    esac
done
shift $((OPTIND - 1))
if [ $# -eq 0 ] ; then
   usage 1
fi
EXE_NAME="$1"
shift

BUILD_DIR="$PWD/build_$ARCH"

# Add path to cpprest dll:
CPPREST_DLL_DIR="$BUILD_DIR/packages/cpprestsdk.2.4.0/build/native/bin/$ARCH/$TOOLS/$CONFIG/Desktop"
PATH="$PATH:$CPPREST_DLL_DIR"
OPENSSL_DLL_DIR="$BUILD_DIR/packages/openssl.redist.1.0.1.25/build/native/bin/$TOOLS/$ARCH/$CONFIG/dynamic"
PATH="$PATH:$OPENSSL_DLL_DIR"

# Add path to boost dlls:
BOOST_ROOT=$(grep "^[ ]*set[ ]*([ ]*BOOST_ROOT" CMake_ext_deps.inc | sed -e 's/.*"\([^"]*\)".*/\1/')
# convert to msys path:
BOOST_ROOT=$(cd "$BOOST_ROOT" ; pwd)
echo "Found BOOST_ROOT=$BOOST_ROOT"
# Convert arch and toolset to boost folder:
BOOST_DLL_DIR="$BOOST_ROOT" # Completed below
case $ARCH in
    Win32)
        BOOST_DLL_DIR="$BOOST_DLL_DIR/lib32"
        ;;
    x64)
        BOOST_DLL_DIR="$BOOST_DLL_DIR/lib64"
        ;;
    ?)
        echo "Invalid arch: $ARCH"
        usage 1
esac
case $TOOLS in
    v120)
        BOOST_DLL_DIR="$BOOST_DLL_DIR-msvc-12.0"
        ;;
    ?)
        echo "Invalid tools: $TOOLS"
        usage 1
esac

PATH="$PATH:$BOOST_DLL_DIR"
echo "PATH=$PATH"

# Where are credentials repositories ?
export PCS_API_REPOSITORY_DIR=$(pwd -W)"/../repositories"
echo "PCS_API_REPOSITORY_DIR=$PCS_API_REPOSITORY_DIR"

# Now we can run executable:
case $EXE_NAME in
    pcs_api_test)
        cd "$BUILD_DIR/libpcs_api/test/$CONFIG"
        ;;
    sample)
        cd "$BUILD_DIR/sample/$CONFIG"
        ;;
    ?)
        echo "Invalid executable name: $EXE_NAME"
        usage 1
esac
echo "PWD=$PWD"
if [ ! -z "$COPY_DLL" ] ; then
    echo "Copying DLLs in folder $PWD"
    cp -u "$CPPREST_DLL_DIR"/*dll .
    cp -u "$CPPREST_DLL_DIR"/*pdb .
    cp -u "$OPENSSL_DLL_DIR"/*dll .
    cp -u "$BOOST_DLL_DIR"/boost_filesystem*dll .
    cp -u "$BOOST_DLL_DIR"/boost_system*dll .
    cp -u "$BOOST_DLL_DIR"/boost_thread*dll .
    cp -u "$BOOST_DLL_DIR"/boost_log*dll .
    cp -u "$BOOST_DLL_DIR"/boost_log_setup*dll .
    cp -u "$BOOST_DLL_DIR"/boost_date_time*dll .
    cp -u "$BOOST_DLL_DIR"/boost_chrono*dll .
    cp -u "$BOOST_DLL_DIR"/boost_program_options*dll .
    echo "Note: not launching $EXE_NAME"
else
    "$EXE_NAME" "$@"
fi


