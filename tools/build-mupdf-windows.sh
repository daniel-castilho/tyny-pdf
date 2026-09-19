#!/usr/bin/env sh
# build-mupdf-windows.sh - Cross-compile MuPDF for Windows x64 using LLVM-MinGW
#
# This script cross-compiles the vendored MuPDF (third_party/mupdf) for Windows x64
# using the pinned LLVM-MinGW toolchain. The output libraries are placed in
# build/mupdf-windows-x64/release/ for use by the win-cross-x64 CMake preset.
#
# Usage: sh tools/build-mupdf-windows.sh [--toolchain-root PATH] [--jobs N]
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

MUPDF_DIR="$ROOT/third_party/mupdf"
OUTPUT_DIR="$ROOT/build/mupdf-windows-x64"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
TOOLCHAIN_ROOT=""

while [ $# -gt 0 ]; do
  case "$1" in
    --toolchain-root) shift; TOOLCHAIN_ROOT=${1:-} ;;
    --jobs) shift; JOBS=${1:-} ;;
    *) echo "build-mupdf-windows.sh: unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

# Resolve toolchain root
if [ -z "$TOOLCHAIN_ROOT" ]; then
  LLVM_MINGW_VERSION="20260812-ucrt-ubuntu-22.04-x86_64"
  TOOLCHAIN_ROOT="$HOME/.toolchains/llvm-mingw-${LLVM_MINGW_VERSION}"
fi

if [ ! -d "$TOOLCHAIN_ROOT/bin" ]; then
  echo "build-mupdf-windows: LLVM-MinGW not found at '$TOOLCHAIN_ROOT'" >&2
  echo "Expected layout: \$HOME/.toolchains/llvm-mingw-20260812-ucrt-ubuntu-22.04-x86_64/bin/x86_64-w64-mingw32-gcc" >&2
  exit 1
fi

export PATH="$TOOLCHAIN_ROOT/bin:$PATH"

# Toolchain nm for the post-build font check (no bare 'nm' in LLVM-MinGW).
NM="$TOOLCHAIN_ROOT/bin/llvm-nm"

# Cross-compilation environment
export CC=x86_64-w64-mingw32-gcc
export CXX=x86_64-w64-mingw32-g++
export AR=x86_64-w64-mingw32-ar
export RANLIB=x86_64-w64-mingw32-ranlib
export STRIP=x86_64-w64-mingw32-strip

# Windows-specific build flags
export XCFLAGS="-DWIN32 -D_WIN32 -DWIN64 -D_WIN64 -D_CRT_SECURE_NO_WARNINGS -msse4.1 -DHAVE_LIBCRYPTO -DFREEGLUT_STATIC -I$OUTPUT_DIR/include -Wno-implicit-function-declaration"
export XLDFLAGS="-static"
export XLIBS="-lkernel32 -luser32 -lgdi32 -lcomdlg32 -ladvapi32 -lws2_32 -lwsock32 -lole32 -loleaut32 -luuid -lversion"

# Output directory
mkdir -p "$OUTPUT_DIR/release"

cd "$MUPDF_DIR"

echo "build-mupdf-windows: building MuPDF for Windows x64 (cross-compile)"
echo "  Toolchain: $TOOLCHAIN_ROOT"
echo "  Output:    $OUTPUT_DIR"
echo "  Jobs:      $JOBS"

# Clean previous build (make clean uses OUT, so it only touches our output dir)
make -j"$JOBS" clean \
  OUT="$OUTPUT_DIR/release" \
  build=release \
  build_prefix="" \
  build_suffix="" \
  2>/dev/null || true

# Build libraries only (no apps). Fonts are embedded via host-side hexdump.sh
# (HAVE_OBJCOPY=no): objcopy cannot relocate binary font data into COFF objects,
# so the Makefile instead generates C arrays on the host and cross-compiles them.
# FONT_BIN/FONT_GEN are therefore left at their Makefile defaults. All system
# libraries are disabled so the vendored thirdparty sources are used; the x11,
# glut and curl viewer apps are not built at all.
MAKE_LOG="${TMPDIR:-/tmp}/tynypdf-mupdf-cross.log"
if make -j"$JOBS" libs libmupdf-threads \
  HAVE_OBJCOPY=no \
  build=release \
  prefix="" \
  build_prefix="" \
  build_suffix="" \
  OUT="$OUTPUT_DIR/release" \
  HAVE_PTHREAD=yes \
  shared=no \
  USE_SYSTEM_LIBS=no \
  USE_SYSTEM_ZLIB=no \
  USE_SYSTEM_JPEG=no \
  USE_SYSTEM_JPEG2000=no \
  USE_SYSTEM_OPENJPEG=no \
  USE_SYSTEM_TIFF=no \
  USE_SYSTEM_PNG=no \
  USE_SYSTEM_FREETYPE=no \
  USE_SYSTEM_HARFBUZZ=no \
  USE_SYSTEM_LCMS2=no \
  USE_SYSTEM_MUJS=no \
  USE_SYSTEM_JBIG2=no \
  HAVE_GLUT=no \
  HAVE_X11=no \
  HAVE_CURL=no \
  HAVE_LIBCRYPTO=no \
  CURL_LIBS="" \
  ZLIB_LIBS="" \
  JPEG_LIBS="" \
  PNG_LIBS="" \
  TIFF_LIBS="" \
  FREETYPE_LIBS="" \
  HARFBUZZ_LIBS="" \
  LCMS2_LIBS="" \
  MUJS_LIBS="" \
  PKCS7_SRC="" \
  LIB_CRYPTO="" \
  LIB_SSL="" \
  >"$MAKE_LOG" 2>&1; then
  :
else
  echo "build-mupdf-windows: FAIL - make exited $? (log tail below)" >&2
  tail -40 "$MAKE_LOG" >&2 || true
  rm -f "$MAKE_LOG"
  exit 1
fi
rm -f "$MAKE_LOG"

# Verify output: the three libraries must exist and libmupdf.a must carry
# embedded font data (fzsymbols from the generated C arrays). A build that
# skipped the font step still links, so a plain existence check is not enough.
FZ_FONT_SYMBOL_COUNT=$("$NM" --defined-only "$OUTPUT_DIR/release/libmupdf.a" 2>/dev/null | grep -cE '_binary_' || true)
if [ -f "$OUTPUT_DIR/release/libmupdf.a" ] && \
   [ -f "$OUTPUT_DIR/release/libmupdf-third.a" ] && \
   [ -f "$OUTPUT_DIR/release/libmupdf-threads.a" ]; then
  if [ "$FZ_FONT_SYMBOL_COUNT" -eq 0 ]; then
    echo "build-mupdf-windows: FAIL - libmupdf.a has no embedded fonts" >&2
    exit 1
  fi
  echo "build-mupdf-windows: SUCCESS - libraries built at $OUTPUT_DIR/release/ ($FZ_FONT_SYMBOL_COUNT embedded fonts)"
  ls -la "$OUTPUT_DIR/release/"*.a
else
  echo "build-mupdf-windows: FAIL - expected libraries not found" >&2
  exit 1
fi
