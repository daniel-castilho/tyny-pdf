# Render layer: portable presentation logic (tile cache, cachemap arrive with story 5.3).
# The DirectX swapchain implementation lives in src/os/win32/swapchain because a Windows or
# DirectX header may not appear under src/render (AGENTS.md rule 1, ADR-0011 R-M10), so this
# library builds a placeholder TU on every platform until story 5.3 adds portable code here.
add_library(tynypdf-render OBJECT
    ${CMAKE_SOURCE_DIR}/src/render/dummy.cc
)

target_include_directories(tynypdf-render PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-render)
pc_apply_warnings(tynypdf-render)

# Alias for backward compatibility
add_library(tynypdf-render-static STATIC $<TARGET_OBJECTS:tynypdf-render>)
