# Test registration. enable_testing() runs from the root; each suite registers itself
# with the ctest preset in CMakePresets.json. Until the suites exist (story 1.4) a ctest
# run reports "no tests were found", which is the defined behaviour of the seed - the
# alternative (a synthetic passing test) would print a green that means nothing.
file(GLOB pc_unit_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cc")

if(NOT pc_unit_sources)
  message(STATUS "tests: no tests yet (unit and contract suites arrive with story 1.4)")
  return()
endif()

foreach(pc_unit_source IN LISTS pc_unit_sources)
  get_filename_component(pc_test_name ${pc_unit_source} NAME_WE)
  add_executable(${pc_test_name} ${pc_unit_source})
  target_link_libraries(${pc_test_name} PRIVATE ${TYNYPDF_LIBRARY_CORE_TARGET})
  pc_apply_warnings(${pc_test_name})
  add_test(NAME ${pc_test_name} COMMAND ${pc_test_name})
endforeach()
