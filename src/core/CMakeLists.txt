# pdfcore is a static library implementing the core IR and public API
add_library(pdfcore STATIC
    geom/geom.cc
    sha256.cc
    sidecar/reader.cc
    sidecar/stale.cc
    sidecar/writer.cc
)

add_subdirectory(budget)

add_subdirectory(selection)

add_subdirectory(search)

add_subdirectory(annot)

add_subdirectory(forms)

# R7.1: pc_doc_open opens through a backend vtable and the core IR is synthesized on the
# null backend (doc.cc), so every pdfcore consumer needs the null backend on its link line.
# Declared PUBLIC here because the doc.cc -> stale.cc chain pulls it in after archive
# re-scans, where a per-test link order cannot reach (story 7.2 hit this via pc_form_fill_field).
target_link_libraries(pdfcore PUBLIC
    tynypdf-budget
    tynypdf-forms
    ${TYNYPDF_LIBRARY_BACKEND_DLL_PREFIX}null
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
