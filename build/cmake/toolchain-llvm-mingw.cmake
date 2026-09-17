# Cross-compilation toolchain for the Windows target, using the pinned LLVM-MinGW
# toolchain (ADR-0010 section 2). This is the only file outside the toolchain itself
# that knows the cross compilers exist; product CMake must stay compiler-agnostic.
#
# The compiler root is resolved in this order:
#   1. -DLLVM_MINGW_ROOT=<path> (what CI passes)
#   2. the LLVM_MINGW_ROOT environment variable
#   3. ${HOME}/.toolchains/llvm-mingw-${LLVM_MINGW_VERSION} (the local layout of
#      docs/dev-environment.md), where LLVM_MINGW_VERSION is pinned in CMakePresets.json
#      and defaults below to the version this repository was created with.
#
# The pin is verified with the documented command:
#   sha256sum ~/.toolchains/llvm-mingw-*/bin/clang | sed 's|.*: ||'
# compared against third_party/toolchains/llvm-mingw.sha256.

if(NOT DEFINED LLVM_MINGW_VERSION)
  set(LLVM_MINGW_VERSION "20260812-ucrt-ubuntu-22.04-x86_64")
endif()

if(DEFINED LLVM_MINGW_ROOT)
  set(llvm_mingw_root "${LLVM_MINGW_ROOT}")
elseif(DEFINED ENV{LLVM_MINGW_ROOT})
  set(llvm_mingw_root "$ENV{LLVM_MINGW_ROOT}")
else()
  set(llvm_mingw_root "${CMAKE_CURRENT_LIST_DIR}/../../../../.toolchains/llvm-mingw-${LLVM_MINGW_VERSION}")
endif()

if(NOT EXISTS "${llvm_mingw_root}/bin")
  message(FATAL_ERROR
    "LLVM-MinGW not found at '${llvm_mingw_root}'. Fetch and verify it first:\n"
    "  curl -L -o /tmp/llvm-mingw.tar.xz \\\n"
    "    https://github.com/mstorsjo/llvm-mingw/releases/download/"
    "20260812/llvm-mingw-20260812-ucrt-ubuntu-22.04-x86_64.tar.xz\n"
    "  tar -xJf /tmp/llvm-mingw.tar.xz -C ${CMAKE_CURRENT_LIST_DIR}/../../../../.toolchains\n"
    "  sha256sum ${CMAKE_CURRENT_LIST_DIR}/../../../../.toolchains/"
    "llvm-mingw-20260812-ucrt-ubuntu-22.04-x86_64/bin/clang\n"
    "  (must match third_party/toolchains/llvm-mingw.sha256), or pass -DLLVM_MINGW_ROOT=...")
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER "${llvm_mingw_root}/bin/x86_64-w64-mingw32-gcc")
set(CMAKE_CXX_COMPILER "${llvm_mingw_root}/bin/x86_64-w64-mingw32-g++")

# The cross toolchain cannot execute Windows binaries at configure time.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Everything the cross compiler needs lives inside its own root (headers and import
# libraries); do not search system paths with the host prefix.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# The static-runtime requirement (ADR-0010 section 2, decision D7b): no VC++ or libstdc++
# redistributable may be part of the shipped binary. -static pulls libgcc, libstdc++ and
# libwinpthread into the portable executable so only system32 DLLs remain (the runtime
# probe verifies this; see tools/win-probe/runtime_probe.c).
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")
