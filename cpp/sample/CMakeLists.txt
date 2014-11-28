################################
# Sample executable
################################
include_directories (${COMMON_INCLUDES}
                     ${CPPREST_INCLUDE_DIR})
link_directories (${CPPREST_LIB_DIR})

add_executable (sample sample.cc)
target_link_libraries (sample pcs_api ${CPPREST_LIB} ${Boost_LIBRARIES})


