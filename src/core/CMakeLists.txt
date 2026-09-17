# The engine-free core (ADR-0002 rule 2; ADR-0011 R-M1..R-M4): the document model, undo,
# sidecar semantics and every geometry decision. Nothing in this directory may include an
# engine header or a Windows header - tools/layering-check.sh (planned, PR #4) gates it.
#
# The pdfcore target exists once there is code; story 1.4 provides the first sources, at
# which point this file needs no edit because the glob is re-evaluated on every build.
file(GLOB_RECURSE pc_core_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

if(NOT pc_core_sources)
  message(STATUS "src/core: no sources yet (${TYNYPDF_LIBRARY_CORE_TARGET} arrives with story 1.4)")
  return()
endif()

add_library(${TYNYPDF_LIBRARY_CORE_TARGET} STATIC ${pc_core_sources})
target_include_directories(${TYNYPDF_LIBRARY_CORE_TARGET} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_SOURCE_DIR}/include)
pc_apply_core_flags(${TYNYPDF_LIBRARY_CORE_TARGET})
