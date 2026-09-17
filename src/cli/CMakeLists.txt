# The headless twin (ADR-0001 rule 4, ADR-0002): every bug must reproduce here first,
# because this is the Linux-side face of everything except presentation. Exit codes
# 0/1/2/3 per ADR-0003 section 6.
file(GLOB pc_cli_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

if(NOT pc_cli_sources)
  message(STATUS "src/cli: no sources yet (${TYNYPDF_EXECUTABLES_CLI} arrives with story 1.4)")
  return()
endif()

add_executable(${TYNYPDF_EXECUTABLES_CLI} ${pc_cli_sources})
target_link_libraries(${TYNYPDF_EXECUTABLES_CLI} PRIVATE ${TYNYPDF_LIBRARY_CORE_TARGET})
pc_apply_warnings(${TYNYPDF_EXECUTABLES_CLI})
