add_library(tynypdf-clipboard STATIC clipboard.cc)

target_link_libraries(tynypdf-clipboard PUBLIC pdfcore)

target_include_directories(tynypdf-clipboard PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

pc_apply_core_flags(tynypdf-clipboard)
pc_apply_warnings(tynypdf-clipboard)
