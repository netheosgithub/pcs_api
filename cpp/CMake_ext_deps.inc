# This file is included by CMakeLists.txt AND scripts runners (for dll paths)
# Adjust paths values to define where dependencies are located
# Keep quotes around paths

# MSVC installs nuget package in sub-dir of build/packages:
set( CPPREST_ROOT  "${PROJECT_BINARY_DIR}/packages/cpprestsdk.2.4.0/build/native" )

if ( WIN32 )
  #set ( Boost_DEBUG ON )
  set( BOOST_ROOT  "e:/Users/lmorel/dev/boost_1_55_0" )
  # In case boost is not found (Windows prebuilt binaries 1.55 are not found):
  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set (BOOST_LIBRARYDIR ${BOOST_ROOT}/lib64-msvc-12.0 )
  else ()
    set (BOOST_LIBRARYDIR ${BOOST_ROOT}/lib32-msvc-12.0 )
  endif()

  # cpprest project location:
  set( CPPREST_RELEASE_DIR ${CPPREST_ROOT} )
endif()

if ( UNIX )
  # installed as per Linux distribution, otherwise define :
  #set( BOOST_ROOT ${BOOST_ROOT_UNIX} )
  #set( BOOST_LIBRARYDIR ${BOOST_ROOT}/lib64-msvc-12.0 )

  # cpprest project location:
  # TODO symbolic link for now: cpp/cpprest -> /where/is/casablanca/Release/
  set( CPPREST_RELEASE_DIR ${PROJECT_SOURCE_DIR}/cpprest )
endif()
