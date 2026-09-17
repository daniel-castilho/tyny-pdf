# render module - tile cache, cachemap, swapchain, Direct2D blit

if (TYNYPDF_EXECUTABLES_RENDER)
    add_executable(${TYNYPDF_EXECUTABLES_RENDER} main.cc)
    target_link_libraries(${TYNYPDF_EXECUTABLES_RENDER} PRIVATE
        pdfcore
        ${TYNYPDF_LIBRARY_RENDER_DLL_PREFIX}render
        ${TYNYPDF_LIBRARY_OS_DLL_PREFIX}win32
    )
    target_include_directories(${TYNYPDF_EXECUTABLES_RENDER} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    pc_apply_core_flags(${TYNYPDF_EXECUTABLES_RENDER})
    pc_apply_warnings(${TYNYPDF_EXECUTABLES_RENDER})
endif()

add_library(${TYNYPDF_LIBRARY_RENDER_DLL_PREFIX}render STATIC
    swapchain.cc
    tile_cache.cc
    cachemap.cc
    )
target_include_directories(${TYNYPDF_LIBRARY_RENDER_DLL_PREFIX}render PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(${TYNYPDF_LIBRARY_RENDER_DLL_PREFIX}render PUBLIC
    ${TYNYPDF_LIBRARY_OS_DLL_PREFIX}win32
)
pc_apply_core_flags(${TYNYPDF_LIBRARY_RENDER_DLL_PREFIX}render)
pc_apply_warnings(${TYNYPDF_LIBRARY_RENDER_DLL_PREFIX}render)
