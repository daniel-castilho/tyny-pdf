# Windows-specific OS implementations
# Only build on Windows or when cross-compiling for Windows
if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR TYNYPDF_CROSS_WINDOWS)
  add_library(tynypdf-os-win32 OBJECT
      window/window.cc
      swapchain/swapchain.cc
  )

  target_include_directories(tynypdf-os-win32 PUBLIC
      ${CMAKE_SOURCE_DIR}/include
  )

  target_link_libraries(tynypdf-os-win32 PUBLIC
      d2d1
      d3d11
      dcomp
      dxgi
  )

  pc_apply_core_flags(tynypdf-os-win32)
  pc_apply_warnings(tynypdf-os-win32)

  # Alias for backward compatibility
  add_library(tynypdf-os-win32-static STATIC $<TARGET_OBJECTS:tynypdf-os-win32>)
else()
  # On Linux, provide empty library
  add_library(tynypdf-os-win32 OBJECT
      ${CMAKE_SOURCE_DIR}/src/os/win32/dummy.cc
  )

  target_include_directories(tynypdf-os-win32 PUBLIC
      ${CMAKE_SOURCE_DIR}/include
  )

  pc_apply_core_flags(tynypdf-os-win32)
  pc_apply_warnings(tynypdf-os-win32)

  # Alias for backward compatibility
  add_library(tynypdf-os-win32-static STATIC $<TARGET_OBJECTS:tynypdf-os-win32>)
endif()
