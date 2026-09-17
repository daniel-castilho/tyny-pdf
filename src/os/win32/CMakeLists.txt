# Win32 OS layer

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_library(${TYNYPDF_LIBRARY_OS_DLL_PREFIX}win32 STATIC
        win32/window.cc
    )
    target_include_directories(${TYNYPDF_LIBRARY_OS_DLL_PREFIX}win32 PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(${TYNYPDF_LIBRARY_OS_DLL_PREFIX}win32 PRIVATE
        dcomp.lib
        d2d1.lib
        d3d11.lib
        dxgi.lib
        dwrite.lib
    )
    pc_apply_core_flags(${TYNYPDF_LIBRARY_OS_DLL_PREFIX}win32)
    pc_apply_warnings(${TYNYPDF_LIBRARY_OS_DLL_PREFIX}win32)
endif()
