# pdfcore is an interface library providing the public API headers
# The actual implementation is in the backend libraries
add_library(pdfcore INTERFACE)
target_include_directories(pdfcore INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_SOURCE_DIR}/include
)

# Create a dummy source file so the target exists
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/pdfcore_dummy.c "int pdfcore_dummy(void) { return 0; }")
add_library(pdfcore_dummy STATIC ${CMAKE_CURRENT_BINARY_DIR}/pdfcore_dummy.c)
target_link_libraries(pdfcore INTERFACE pdfcore_dummy)
