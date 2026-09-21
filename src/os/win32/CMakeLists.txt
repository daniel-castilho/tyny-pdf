# Windows-specific OS implementations
add_library(tynypdf-os-win32 OBJECT
    sidecar_lock.cc
)

target_include_directories(tynypdf-os-win32 PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-os-win32)
pc_apply_warnings(tynypdf-os-win32)

# Alias for backward compatibility
add_library(tynypdf-os-win32-static STATIC $<TARGET_OBJECTS:tynypdf-os-win32>)
