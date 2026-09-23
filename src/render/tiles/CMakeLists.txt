# Tile cache implementation
# Only build on Windows or when cross-compiling for Windows (has DirectX deps)
# For Linux headless tests, we build a minimal version

if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR TYNYPDF_CROSS_WINDOWS)
  add_library(tynypdf-tiles OBJECT
      tiles.cc
  )

  target_include_directories(tynypdf-tiles PUBLIC
      ${CMAKE_SOURCE_DIR}/include
  )

  target_link_libraries(tynypdf-tiles PUBLIC
      pdfcore
      tynypdf-budget
  )

  pc_apply_core_flags(tynypdf-tiles)
  pc_apply_warnings(tynypdf-tiles)

  add_library(tynypdf-tiles-static STATIC $<TARGET_OBJECTS:tynypdf-tiles>)
else()
  # On Linux, provide a minimal implementation for headless tests
  add_library(tynypdf-tiles OBJECT
      tiles.cc
  )

  target_include_directories(tynypdf-tiles PUBLIC
      ${CMAKE_SOURCE_DIR}/include
  )

  target_link_libraries(tynypdf-tiles PUBLIC
      pdfcore
      tynypdf-budget
  )

  pc_apply_core_flags(tynypdf-tiles)
  pc_apply_warnings(tynypdf-tiles)

  add_library(tynypdf-tiles-static STATIC $<TARGET_OBJECTS:tynypdf-tiles>)
endif()
