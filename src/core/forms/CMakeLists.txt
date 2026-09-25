add_library(tynypdf-forms STATIC
    forms.cc
)

target_link_libraries(tynypdf-forms PUBLIC
    pdfcore
)

target_include_directories(tynypdf-forms PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-forms)
pc_apply_warnings(tynypdf-forms)
