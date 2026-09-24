add_library(tynypdf-annot STATIC
    annot.cc
)

target_link_libraries(tynypdf-annot PUBLIC
    pdfcore
)

target_include_directories(tynypdf-annot PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-annot)
pc_apply_warnings(tynypdf-annot)
