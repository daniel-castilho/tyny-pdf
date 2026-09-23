# Render layer: portable presentation logic (swapchain in os/win32, tiles/cachemap here).
# The DirectX swapchain implementation lives in src/os/win32/swapchain because rule 1
# forbids a Windows header under src/render (ADR-0011 R-M10). This library builds the
# portable tiles/cachemap on every platform.

add_subdirectory(tiles)
add_subdirectory(cachemap)

add_library(tynypdf-render OBJECT
    ${CMAKE_SOURCE_DIR}/src/render/dummy.cc
)

target_include_directories(tynypdf-render PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(tynypdf-render PUBLIC
    tynypdf-tiles
    tynypdf-cachemap
    pdfcore
    tynypdf-budget
)

pc_apply_core_flags(tynypdf-render)
pc_apply_warnings(tynypdf-render)

# Alias for backward compatibility
add_library(tynypdf-render-static STATIC $<TARGET_OBJECTS:tynypdf-render>)
