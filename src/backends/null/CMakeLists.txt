# The null backend: a pure-pdfcore implementation of the versioned vtable in
# include/pdfcore/backend.h (story 1.4), used by the tests and by tynypdf-cli. Named by
# what it is, never by what it is not (ADR-0002 rule 4).
file(GLOB pc_null_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

if(NOT pc_null_sources)
  message(STATUS "src/backends/null: no sources yet (arrives with story 1.4)")
  return()
endif()

add_library(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null STATIC ${pc_null_sources})
target_include_directories(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null PRIVATE
  ${TYNYPDF_LIBRARY_CORE_TARGET})
pc_apply_core_flags(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
