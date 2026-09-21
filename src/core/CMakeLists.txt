# pdfcore is a static library implementing the core IR and public API
add_library(pdfcore STATIC
    doc/doc.cc
    geom/geom.cc
    sha256.cc
)

target_include_directories(pdfcore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(pdfcore)
pc_apply_warnings(pdfcore)
