# Linux-specific OS implementations
add_library(tynypdf-os-linux OBJECT
    sidecar_lock.cc
)

target_include_directories(tynypdf-os-linux PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-os-linux)
pc_apply_warnings(tynypdf-os-linux)

# Alias for backward compatibility
add_library(tynypdf-os-linux-static STATIC $<TARGET_OBJECTS:tynypdf-os-linux>)
