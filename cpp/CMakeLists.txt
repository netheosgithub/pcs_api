###############################
# pcs_api top CMakeLists.txt
###############################

cmake_minimum_required(VERSION 2.8)

# Make PROJECT_SOURCE_DIR, PROJECT_BINARY_DIR, and PROJECT_NAME available.
set(PROJECT_NAME pcs_api)
project(${PROJECT_NAME})

# The version number.
set (PCS_API_VERSION_MAJOR 0)
set (PCS_API_VERSION_MINOR 1)

# If you want your own include/ directory, set this, and then you can do
# include_directories(${COMMON_INCLUDES}) in other CMakeLists.txt files.
set(COMMON_INCLUDES ${PROJECT_SOURCE_DIR}/libpcs_api/include)


if(MSVC)
  #add_definitions( /W3 )
  message(STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
endif()

if( CMAKE_COMPILER_IS_GNUCC )
  add_definitions(-std=c++11)
  add_definitions(-Wall)
  add_definitions(-Wno-unknown-pragmas -Wno-reorder -Wno-unused-local-typedefs)  # avoids cpprestsdk warnings
endif()

add_definitions( -D_REENTRANT -DBOOST_ALL_DYN_LINK)

################################
# Dependencies
################################

# Dependencies locations are located in a single external file:
include ( ${PROJECT_SOURCE_DIR}/CMake_ext_deps.inc OPTIONAL )

#SET(Boost_USE_STATIC_LIBS ON)
FIND_PACKAGE( Boost 1.55 COMPONENTS
       filesystem
       system
       thread
       log
       log_setup
       date_time
       chrono
       program_options
   REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

# cpprest does not provide FindCpprest.cmake for now
# (see http://casablanca.codeplex.com/workitem/59)
# so we "import" the binaries, according to nuget package folders structure:
# see https://casablanca.codeplex.com/wikipage?title=Versioning&referringTitle=Documentation
add_library(cpprest SHARED IMPORTED)
if ( WIN32 )
  # We suppose here that the cpprest nuGET package has been installed
  set(CPPREST_INCLUDE_DIR ${CPPREST_RELEASE_DIR}/include)
  # Determine actual names of lib and dll: depends on arch and debug/release
  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set (CPPREST_ARCH_DIR x64)
  else ()
    set (CPPREST_ARCH_DIR Win32)
  endif()
  set_target_properties(cpprest PROPERTIES
          IMPORTED_CONFIGURATIONS "DEBUG;OPTIMIZED;"
          IMPORTED_IMPLIB_DEBUG ${CPPREST_RELEASE_DIR}/lib/${CPPREST_ARCH_DIR}/v120/Debug/Desktop/cpprest120d_2_3.lib
          IMPORTED_LOCATION_DEBUG ${CPPREST_RELEASE_DIR}/bin/${CPPREST_ARCH_DIR}/v120/Debug/Desktop/cpprest120d_2_3.dll
          IMPORTED_IMPLIB_RELEASE ${CPPREST_RELEASE_DIR}/lib/${CPPREST_ARCH_DIR}/v120/Release/Desktop/cpprest120_2_3.lib
          IMPORTED_LOCATION_RELEASE ${CPPREST_RELEASE_DIR}/bin/${CPPREST_ARCH_DIR}/v120/Release/Desktop/cpprest120_2_3.dll
	  )
endif()
if ( UNIX )
  set(CPPREST_INCLUDE_DIR ${CPPREST_RELEASE_DIR}/include)
  set_target_properties(cpprest PROPERTIES
          IMPORTED_CONFIGURATIONS "DEBUG;OPTIMIZED;"
          IMPORTED_LOCATION ${CPPREST_RELEASE_DIR}/build.release/Binaries/libcpprest.so
	  )
endif()

set(GTEST_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/third_party/gtest/include)
if ( MSVC )
    # force this option to ON so that Google Test will use /MD instead of /MT
    # /MD is now the default for Visual Studio, so it should be our default, too
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared (DLL) run-time lib even when Google Test is built as static lib.")
endif()

################################
# Normal Libraries & Executables
################################

add_subdirectory(third_party/gtest)
add_subdirectory(libpcs_api)
add_subdirectory(sample)

