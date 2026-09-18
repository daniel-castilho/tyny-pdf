# MuPDF backend - builds against the vendored engine in third_party/mupdf/
# Enabled when MuPDF libraries are available (native or cross-compiled).

# Check for native build first
if (EXISTS ${CMAKE_SOURCE_DIR}/third_party/mupdf/build/release/libmupdf.a AND NOT CMAKE_CROSSCOMPILING)
    set(MUPDF_LIB_DIR "${CMAKE_SOURCE_DIR}/third_party/mupdf/build/release")
# Check for cross-compiled Windows build
elseif (EXISTS ${CMAKE_SOURCE_DIR}/${MUPDF_BUILD_DIR}/release/libmupdf.a)
    set(MUPDF_LIB_DIR "${CMAKE_SOURCE_DIR}/${MUPDF_BUILD_DIR}/release")
else()
    set(MUPDF_LIB_DIR "")
endif()

if (MUPDF_LIB_DIR)
    add_library(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf STATIC mupdf_backend.cc)
    target_include_directories(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/mupdf/include
        ${CMAKE_SOURCE_DIR}/third_party/mupdf/thirdparty
    )
    target_link_libraries(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf PUBLIC
        ${MUPDF_LIB_DIR}/libmupdf.a
        ${MUPDF_LIB_DIR}/libmupdf-third.a
        ${MUPDF_LIB_DIR}/libmupdf-threads.a
    )
    pc_apply_core_flags(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
    pc_apply_warnings(${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}mupdf)
else()
    if (CMAKE_CROSSCOMPILING)
        message(STATUS "MuPDF backend skipped during cross-compilation (requires cross-compiled MuPDF at ${MUPDF_BUILD_DIR})")
    else()
        message(WARNING "MuPDF not built, skipping mupdf backend. Run 'make build=release HAVE_PTHREAD=yes -j8' in third_party/mupdf/")
    endif()
endif()
