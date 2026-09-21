#!/usr/bin/env sh
# build-mupdf-linux.sh - Build the vendored MuPDF (third_party/mupdf) natively for Linux.
#
# The native build exists because the §3.5 contract final proofs (contract against the mupdf
# backend, the per-doc lock harden proof and the threaded R-M7 benchmark) must run on the Linux
# tree, the same place the core is built and tested. It mirrors build-mupdf-windows.sh: libraries
# only, all system libraries disabled, pthreads enabled so libmupdf-threads.a (the per-context lock
# set MuPDF uses) exists. Output lands under build/ so the working tree stays clean.
#
# Usage: sh tools/build-mupdf-linux.sh [--jobs N] [--output-dir DIR]
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

MUPDF_DIR="$ROOT/third_party/mupdf"
OUTPUT_DIR="$ROOT/build/mupdf-linux-x64"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

while [ $# -gt 0 ]; do
  case "$1" in
    --jobs) shift; JOBS=${1:-} ;;
    --output-dir) shift; OUTPUT_DIR=${1:-} ;;
    *) echo "build-mupdf-linux.sh: unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

mkdir -p "$OUTPUT_DIR/release"

cd "$MUPDF_DIR"

echo "build-mupdf-linux: building MuPDF natively (libraries only)"
echo "  Output:    $OUTPUT_DIR"
echo "  Jobs:      $JOBS"

# Libraries only, no apps; every vendored thirdparty source is used so no host system library
# leaks into the link. HAVE_OBJCOPY=no generates the embedded font data as C arrays on the host.
MAKE_LOG="${TMPDIR:-/tmp}/tynypdf-mupdf-native.log"
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
  echo "build-mupdf-linux: FAIL - make exited $? (log tail below)" >&2
  tail -50 "$MAKE_LOG" >&2 || true
  rm -f "$MAKE_LOG"
  exit 1
fi
rm -f "$MAKE_LOG"

if [ -f "$OUTPUT_DIR/release/libmupdf.a" ] && \
   [ -f "$OUTPUT_DIR/release/libmupdf-third.a" ] && \
   [ -f "$OUTPUT_DIR/release/libmupdf-threads.a" ]; then
  echo "build-mupdf-linux: SUCCESS - libraries built at $OUTPUT_DIR/release/"
  ls -la "$OUTPUT_DIR/release/"*.a
else
  echo "build-mupdf-linux: FAIL - expected libraries not found" >&2
  exit 1
fi
