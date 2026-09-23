# pdfcore is a static library implementing the core IR and public API
add_library(pdfcore STATIC
    geom/geom.cc
    sha256.cc
    sidecar/reader.cc
    sidecar/stale.cc
    sidecar/writer.cc
)

add_subdirectory(budget)

target_link_libraries(pdfcore PUBLIC
    tynypdf-budget
)

add_subdirectory(doc)
add_subdirectory(json)
add_subdirectory(text)

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
