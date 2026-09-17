#!/usr/bin/env sh
# Build the win-probe executables through the documented presets and print the command
# each probe needs to answer its question (docs/dev-environment.md section 8:
# "the four ASK questions of ADR-0010 section 2, answered by running ... probes").
set -eu
cd "$(dirname "$0")/../.."

cmake --preset linux-core >/dev/null
cmake --build --preset linux-core --target tynypdf-abi-probe

cmake --preset win-cross-x64 >/dev/null
cmake --build --preset win-cross-x64 --target winprobe_test

# Static runtime is a property of the import table; objdump reads a PE as easily as an ELF.
echo "runtime (imports): objdump -p build/win-cross-x64/Release/winprobe_runtime.exe | grep 'DLL Name'"
echo "d2d surface:       ./build/win-cross-x64/Release/winprobe_d2d.exe"
echo "gpu adapter:       ./build/win-cross-x64/Release/winprobe_gpu.exe"
echo "abi (host/cross):  nm -C --defined-only build/linux-core/Debug/libtynypdf-abi-probe.a   vs"
echo "                    nm -C --defined-only build/win-cross-x64/Release/libtynypdf-abi-probe.a"
