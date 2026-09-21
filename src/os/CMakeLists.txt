# OS abstraction layer
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_subdirectory(win32)
  set(TYNYPDF_OS_LIB tynypdf-os-win32)
else()
  add_subdirectory(linux)
  set(TYNYPDF_OS_LIB tynypdf-os-linux)
endif()

# Create a static library that pulls in the OS objects
add_library(tynypdf-os STATIC $<TARGET_OBJECTS:${TYNYPDF_OS_LIB}>)
target_include_directories(tynypdf-os PUBLIC ${CMAKE_SOURCE_DIR}/include)
