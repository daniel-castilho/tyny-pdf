add_library(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null STATIC null_backend.cc)
target_include_directories(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
pc_apply_core_flags(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
pc_apply_warnings(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
