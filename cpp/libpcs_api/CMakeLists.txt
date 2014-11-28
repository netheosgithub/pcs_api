
# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
  "${PROJECT_BINARY_DIR}/config.h"
  )


################################
# Normal Libraries & Executables
################################
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include
                     ${CPPREST_INCLUDE_DIR}
                   )

SET(pcs_api_srcs
    src/bytesio/file_byte_source.cc
    src/bytesio/memory_byte_source.cc
    src/bytesio/progress_byte_source.cc
    src/bytesio/file_byte_sink.cc
    src/bytesio/memory_byte_sink.cc
    src/bytesio/progress_byte_sink.cc
    src/bytesio/stdout_progress_listener.cc
    src/credentials/app_info.cc
    src/credentials/app_info_file_repository.cc
    src/credentials/credentials.cc
    src/credentials/oauth2_app_info.cc
    src/credentials/oauth2_credentials.cc
    src/credentials/password_credentials.cc
    src/credentials/user_credentials.cc
    src/credentials/user_credentials_file_repository.cc
    src/model/c_blob.cc
    src/model/c_download_request.cc
    src/model/c_exceptions.cc
    src/model/c_file.cc
    src/model/c_folder.cc
    src/model/c_folder_content.cc
    src/model/c_folder_content_builder.cc
    src/model/c_path.cc
    src/model/c_quota.cc
    src/model/c_upload_request.cc
    src/model/retry_strategy.cc
    src/storage/storage_facade.cc
    src/storage/storage_builder.cc
    src/storage/utilities.cc
    src/providers/cloudme.cc
    src/providers/dropbox.cc
    src/providers/googledrive.cc
    src/providers/hubic.cc
    src/providers/swift_client.cc
    src/request/c_response.cc
    src/request/request_invoker.cc
    src/request/retry_401_once_response_validator.cc
    src/request/form_body_builder.cc
    src/request/multipart_streamer.cc
    src/request/multipart_streambuf.cc
    src/request/http_client_pool.cc
    src/request/json_utils.cc
    src/request/uri_utils.cc
    src/auth/oauth2.cc
    src/auth/oauth2_session_manager.cc
    src/auth/oauth2_bootstrapper.cc
    src/auth/password_session_manager.cc
   )

SET(pcs_api_hdrs
    include/pcs_api/app_info.h
    include/pcs_api/app_info_file_repository.h
    include/pcs_api/app_info_repository.h
    include/pcs_api/byte_sink.h
    include/pcs_api/byte_source.h
    include/pcs_api/c_blob.h
    include/pcs_api/c_download_request.h
    include/pcs_api/c_exceptions.h
    include/pcs_api/c_file.h
    include/pcs_api/c_folder.h
    include/pcs_api/c_folder_content.h
    include/pcs_api/c_path.h
    include/pcs_api/c_quota.h
    include/pcs_api/c_upload_request.h
    include/pcs_api/credentials.h
    include/pcs_api/file_byte_sink.h
    include/pcs_api/file_byte_source.h
    include/pcs_api/i_storage_provider.h
    include/pcs_api/memory_byte_sink.h
    include/pcs_api/memory_byte_source.h
    include/pcs_api/model.h
    include/pcs_api/oauth2_app_info.h
    include/pcs_api/oauth2_bootstrapper.h
    include/pcs_api/oauth2_credentials.h
    include/pcs_api/password_credentials.h
    include/pcs_api/progress_listener.h
    include/pcs_api/retry_strategy.h
    include/pcs_api/stdout_progress_listener.h
    include/pcs_api/storage_builder.h
    include/pcs_api/storage_facade.h
    include/pcs_api/types.h
    include/pcs_api/user_credentials.h
    include/pcs_api/user_credentials_file_repository.h
    include/pcs_api/user_credentials_repository.h
    include/pcs_api/internal/c_folder_content_builder.h
    include/pcs_api/internal/c_response.h
    include/pcs_api/internal/dll_defines.h
    include/pcs_api/internal/form_body_builder.h
    include/pcs_api/internal/json_utils.h
    include/pcs_api/internal/logger.h
    include/pcs_api/internal/multipart_streambuf.h
    include/pcs_api/internal/multipart_streamer.h
    include/pcs_api/internal/oauth2.h
    include/pcs_api/internal/oauth2_session_manager.h
    include/pcs_api/internal/oauth2_storage_provider.h
    include/pcs_api/internal/object_pool.h
    include/pcs_api/internal/http_client_pool.h
    include/pcs_api/internal/password_session_manager.h
    include/pcs_api/internal/password_storage_provider.h
    include/pcs_api/internal/progress_byte_sink.h
    include/pcs_api/internal/progress_byte_source.h
    include/pcs_api/internal/request_invoker.h
    include/pcs_api/internal/retry_401_once_response_validator.h
    include/pcs_api/internal/storage_provider.h
    include/pcs_api/internal/uri_utils.h
    include/pcs_api/internal/utilities.h
    include/pcs_api/internal/providers/cloudme.h
    include/pcs_api/internal/providers/dropbox.h
    include/pcs_api/internal/providers/googledrive.h
    include/pcs_api/internal/providers/hubic.h
    include/pcs_api/internal/providers/swift_client.h
   )

SET(pcs_api_srcs
    ${pcs_api_srcs}
    ${pcs_api_hdrs}
   )

add_library(pcs_api STATIC
            ${pcs_api_srcs}
           )
target_link_libraries(pcs_api cpprest)

add_subdirectory(test)


