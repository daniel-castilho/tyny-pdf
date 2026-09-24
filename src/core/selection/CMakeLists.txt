add_library(tynypdf-selection STATIC
    selection.cc
)

target_link_libraries(tynypdf-selection PUBLIC
    pdfcore
)

target_include_directories(tynypdf-selection PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-selection)
pc_apply_warnings(tynypdf-selection)
