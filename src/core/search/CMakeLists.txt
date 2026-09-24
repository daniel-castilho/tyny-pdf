add_library(tynypdf-search STATIC
    search.cc
)

target_link_libraries(tynypdf-search PUBLIC
    pdfcore
)

target_include_directories(tynypdf-search PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-search)
pc_apply_warnings(tynypdf-search)
