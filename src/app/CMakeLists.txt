if (TYNYPDF_EXECUTABLES_VIEWER)
    set(TYNYPDF_VIEWER_SOURCES main.cc viewer/viewer.cc)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR TYNYPDF_CROSS_WINDOWS)
        # The D2D blit and the bench mode are Windows-only (story 1.5, R31.1);
        # the platform-neutral loop in viewer.cc still builds everywhere so the
        # headless contract test exercises the same code the viewer runs.
        list(APPEND TYNYPDF_VIEWER_SOURCES viewer/d2d_blit.cc bench/bench.cc)
    endif()
    add_executable(${TYNYPDF_EXECUTABLES_VIEWER} ${TYNYPDF_VIEWER_SOURCES})
    target_link_libraries(${TYNYPDF_EXECUTABLES_VIEWER} PRIVATE pdfcore
        tynypdf-render tynypdf-tiles tynypdf-cachemap tynypdf-os-win32
        ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR TYNYPDF_CROSS_WINDOWS)
        target_link_libraries(${TYNYPDF_EXECUTABLES_VIEWER} PRIVATE psapi)
    endif()
    if (TARGET ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_link_libraries(${TYNYPDF_EXECUTABLES_VIEWER} PRIVATE ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
        target_compile_definitions(${TYNYPDF_EXECUTABLES_VIEWER} PRIVATE PC_HAVE_MUPDF)
    endif()
    target_include_directories(${TYNYPDF_EXECUTABLES_VIEWER} PRIVATE ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/app)
    pc_apply_core_flags(${TYNYPDF_EXECUTABLES_VIEWER})
    pc_apply_warnings(${TYNYPDF_EXECUTABLES_VIEWER})
endif()
