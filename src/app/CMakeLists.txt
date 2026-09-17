# The viewer executable: a thin composition root only (ADR-0002 rule 3). No document
# semantics live here; anything a bug needs to reproduce land in src/core.
file(GLOB pc_app_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

if(NOT pc_app_sources)
  message(STATUS "src/app: no sources yet (${TYNYPDF_EXECUTABLES_VIEWER} arrives with story 1.4)")
  return()
endif()

add_executable(${TYNYPDF_EXECUTABLES_VIEWER} ${pc_app_sources})
target_link_libraries(${TYNYPDF_EXECUTABLES_VIEWER} PRIVATE ${TYNYPDF_LIBRARY_CORE_TARGET})
pc_apply_warnings(${TYNYPDF_EXECUTABLES_VIEWER})
