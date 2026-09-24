# Linux OS abstraction layer
add_library(tynypdf-os-linux OBJECT
    sidecar_lock.cc
    clipboard/clipboard.cc
)

target_include_directories(tynypdf-os-linux PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tynypdf-os-linux PUBLIC
    pthread
)

pc_apply_core_flags(tynypdf-os-linux)
pc_apply_warnings(tynypdf-os-linux)

# Alias for backward compatibility
add_library(tynypdf-os-linux-static STATIC $<TARGET_OBJECTS:tynypdf-os-linux>)

add_subdirectory(clipboard)
