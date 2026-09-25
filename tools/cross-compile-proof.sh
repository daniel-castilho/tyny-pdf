#!/bin/bash
# R52.1 (story 7.3) - cross-compile proof: the Win32 input plumbing (WM_CHAR
# forwarding, the key/char callbacks, the composition root's announcement sync)
# only compiles under the MinGW cross preset; this script is the repeatable
# artefact for that claim. Fails on any compile error, prints the binary path.
# Runs under sh (dash): no pipefail, plain set -eu.
set -eu
cd "$(dirname "$0")/.."

if [ ! -d build/mupdf-windows-x64 ]; then
    echo "cross-compile-proof: missing cross MuPDF (run tools/build-mupdf-windows.sh first)" >&2
    exit 1
fi

cmake --build --preset win-cross-x64 >/tmp/tynypdf-cross.log 2>&1 || {
    grep -E "error" /tmp/tynypdf-cross.log | head -20 >&2
    echo "cross-compile-proof: FAILED (full log: /tmp/tynypdf-cross.log)" >&2
    exit 1
}

if [ ! -f build/win-cross-x64/Release/tynypdf.exe ]; then
    echo "cross-compile-proof: tynypdf.exe not produced" >&2
    exit 1
fi

echo "cross-compile-proof: OK ($(ls -la build/win-cross-x64/Release/tynypdf.exe | awk '{print $5}') bytes)"
exit 0
