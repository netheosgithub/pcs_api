

################################
# Normal Libraries & Executables
################################
include_directories( ${GTEST_INCLUDE_DIR}
                     ${CPPREST_INCLUDE_DIR} )

set( parent_boost_libs ${Boost_LIBRARIES} )
FIND_PACKAGE( Boost 1.55 COMPONENTS
       regex  # required by asio
   REQUIRED )
set( Boost_LIBRARIES ${parent_boost_libs} ${Boost_LIBRARIES} )

SET(pcs_api_test_srcs
    functional_base_test.cc
    basic_test.cc
    stress_test.cc
    misc_test_utils.cc
    models_test.cc
    bytesio_test.cc
    utils_test.cc
    fixed_buffer_byte_sink.cc
    bad_memory_byte_source.cc
    multipart_streamer_test.cc
    multipart_streambuf_test.cc
    swift_test.cc
    test_main.cc
)

SET(pcs_api_test_hdrs
    functional_base_test.h
    misc_test_utils.h
    fixed_buffer_byte_sink.h
    bad_memory_byte_source.h
)

add_executable(pcs_api_test
        ${pcs_api_test_srcs}
        ${pcs_api_test_hdrs}
)
target_link_libraries (pcs_api_test pcs_api cpprest ${Boost_LIBRARIES} gtest)

# Creating launchers greatly helps development,
# as path needs to be configured, etc.:
# TODO work in progress
#list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/third_party/cmake-modules")
#include( CreateLaunchers )
#guess_runtime_library_dirs( pcs_api_test pcs_api cpprest ${Boost_LIBRARIES} gtest)
#create_target_launcher( pcs_api_test
#   RUNTIME_LIBRARY_DIRS pcs_api gtest cpprest
#   WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/lib_pcs_api/test
#)


