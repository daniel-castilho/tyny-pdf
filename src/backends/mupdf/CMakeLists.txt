# MuPDF backend - builds against the vendored engine in third_party/mupdf/
# Only enabled when MuPDF is available and configured, and NOT cross-compiling.

if (EXISTS ${CMAKE_SOURCE_DIR}/third_party/mupdf/build/release/libmupdf.a AND NOT CMAKE_CROSSCOMPILING)
    add_library(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf STATIC mupdf_backend.cc)
    target_include_directories(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/mupdf/include
        ${CMAKE_SOURCE_DIR}/third_party/mupdf/thirdparty
    )
    target_link_libraries(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/mupdf/build/release/libmupdf.a
        ${CMAKE_SOURCE_DIR}/third_party/mupdf/build/release/libmupdf-third.a
        ${CMAKE_SOURCE_DIR}/third_party/mupdf/build/release/libmupdf-threads.a
    )
    pc_apply_core_flags(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
    pc_apply_warnings(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
elseif (CMAKE_CROSSCOMPILING)
    message(STATUS "MuPDF backend skipped during cross-compilation (requires native build or cross-compiled MuPDF)")
else()
    message(WARNING "MuPDF not built, skipping mupdf backend. Run 'make build=release HAVE_PTHREAD=yes -j8' in third_party/mupdf/")
endif()
