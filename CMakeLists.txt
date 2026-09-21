cmake_minimum_required(VERSION 3.30)
project(tyny_pdf VERSION 0.1.0 LANGUAGES C CXX)

include(generated/naming.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)

option(TYNYPDF_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
option(TYNYPDF_BUILD_TESTS "Build tests" ON)

function(pc_apply_warnings target)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
endfunction()

function(pc_apply_core_flags target)
    target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    set_target_properties(${target} PROPERTIES C_STANDARD 11 CXX_STANDARD 20)
endfunction()

add_subdirectory(src)
add_subdirectory(src/os)
add_subdirectory(tools/win-probe)

if (TYNYPDF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_subdirectory(docs)
