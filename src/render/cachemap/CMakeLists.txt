# Cachemap implementation
# Only build on Windows or when cross-compiling for Windows
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR TYNYPDF_CROSS_WINDOWS)
  add_library(tynypdf-cachemap OBJECT
      cachemap.cc
  )

  target_include_directories(tynypdf-cachemap PUBLIC
      ${CMAKE_SOURCE_DIR}/include
  )

  target_link_libraries(tynypdf-cachemap PUBLIC
      pdfcore
      tynypdf-tiles
      tynypdf-budget
  )

  pc_apply_core_flags(tynypdf-cachemap)
  pc_apply_warnings(tynypdf-cachemap)

  add_library(tynypdf-cachemap-static STATIC $<TARGET_OBJECTS:tynypdf-cachemap>)
else()
  # On Linux, provide a minimal implementation for headless tests
  add_library(tynypdf-cachemap OBJECT
      cachemap.cc
  )

  target_include_directories(tynypdf-cachemap PUBLIC
      ${CMAKE_SOURCE_DIR}/include
  )

  target_link_libraries(tynypdf-cachemap PUBLIC
      pdfcore
      tynypdf-tiles
      tynypdf-budget
  )

  pc_apply_core_flags(tynypdf-cachemap)
  pc_apply_warnings(tynypdf-cachemap)

  add_library(tynypdf-cachemap-static STATIC $<TARGET_OBJECTS:tynypdf-cachemap>)
endif()
