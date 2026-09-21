# pdfcore is a static library implementing the core IR and public API
add_library(pdfcore STATIC
    doc/doc.cc
    doc/transaction.cc
    annotation/annotation.cc
    geom/geom.cc
    sha256.cc
    budget/budget.cc
    sidecar/writer.cc
    text/normalize.cc
    text/break.cc
    text/caret.cc
    text/text.cc
)

target_include_directories(pdfcore PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(pdfcore)
pc_apply_warnings(pdfcore)
