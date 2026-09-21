# pdfcore is a static library implementing the core IR and public API
add_library(pdfcore STATIC
    doc/doc.cc
    geom/geom.cc
    sha256.cc
    sidecar/writer.cc
)

# Platform-specific OS implementations for sidecar lock
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(pdfcore PRIVATE ${CMAKE_SOURCE_DIR}/src/os/win32/sidecar_lock.cc)
else()
    target_sources(pdfcore PRIVATE ${CMAKE_SOURCE_DIR}/src/os/linux/sidecar_lock.cc)
endif()

target_include_directories(pdfcore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(pdfcore)
pc_apply_warnings(pdfcore)
