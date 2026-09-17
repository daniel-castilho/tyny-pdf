# Tyny PDF build system. Arrives with PR #2 together with CMakePresets.json, the style
# configs, tools/win-probe/ and the CI matrix that runs these presets (the pairing is
# recorded in the header comment of .github/workflows/gates.yml).
#
# Layout follows ADR-0002: one directory per vertical slice under src/, horizontal seams
# where a second implementation is expected. Targets are conditionally added when their
# source files exist, so this tree configures cleanly while it still has no product code;
# the first real targets are the win-probe executables (tools/win-probe/) and the product
# targets arrive with story 1.4.
#
# Layering rule from ADR-0002 rules 2 and 4: no file outside src/backends/ may name an
# engine. tools/layering-check.sh (planned, PR #4) turns that sentence into a gate.

cmake_minimum_required(VERSION 3.30)

project(tyny_pdf VERSION 0.1.0 LANGUAGES C CXX)

# The single source of product identifiers: docs/naming.md, regenerated with
# tools/naming-sync.py write. Never edit the generated values by hand (AGENTS.md rule 8).
include(${CMAKE_CURRENT_SOURCE_DIR}/generated/naming.cmake)

# The version lives here and nowhere else (docs/release-runbook.md, "one place declares
# the version").
set(TYNYPDF_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(TYNYPDF_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(TYNYPDF_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# ---------------------------------------------------------------------------
# Output layout: every built artifact lands in <build-dir>/<config>/ so the documented
# paths hold for both single-config (Ninja) and multi-config (Visual Studio) generators.
# The release contract is: build/win-cross-x64/Release/tynypdf(.exe).
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>)

# ---------------------------------------------------------------------------
# Warnings are errors on both toolchains (AGENTS.md, ADR-0010 negative consequences).
# GNU/Clang: -Wall -Wextra -Wpedantic -Werror; MSVC: /W4 /WX. A suppression needs a
# comment naming the diagnostic id and why it is wrong here.
# ---------------------------------------------------------------------------
option(TYNYPDF_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

function(pc_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(TYNYPDF_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Core flags shared by the engine-facing C and C++ (dependency-policy.md section 7:
# -fno-exceptions -fno-rtti on src/core and src/backends/**, C ABI per ADR-0003). These
# are target-level on purpose: tests and the win probes keep the runtime features the
# test framework needs, so the flags must not leak into a global CMAKE_CXX_FLAGS.
# ---------------------------------------------------------------------------
set(TYNYPDF_CORE_CXX_FLAGS -fno-exceptions -fno-rtti)

function(pc_apply_core_flags target)
  target_compile_features(${target} PRIVATE c_std_11 cxx_std_20)
  if(NOT MSVC)
    target_compile_options(${target} PRIVATE ${TYNYPDF_CORE_CXX_FLAGS})
  endif()
  pc_apply_warnings(${target})
endfunction()

add_subdirectory(src)
add_subdirectory(tools/win-probe)

enable_testing()
add_subdirectory(tests)
add_subdirectory(docs)
