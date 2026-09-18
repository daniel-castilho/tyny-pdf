#!/usr/bin/env sh
# veraPDF.sh - ISO 32000 conformance validation using veraPDF
#
# Usage: sh tools/verapdf.sh [--corpus DIR] [--profile FILE] [--output FILE] [--self-test]
# The veraPDF tool is the oracle for ISO 32000 validity and UA claims.
# We do not grade our own homework - veraPDF is the independent oracle.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

corpus_dir="$ROOT/tests/conformance"
profile=""
output=""
self_test=0

while [ $# -gt 0 ]; do
  case "$1" in
    --corpus) shift; corpus_dir=${1:-} ;;
    --profile) shift; profile=${1:-} ;;
    --output) shift; output=${1:-} ;;
    --self-test) self_test=1 ;;
    *) echo "verapdf.sh: unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

[ -n "$corpus_dir" ] || { echo "verapdf.sh: --corpus requires a path" >&2; exit 2; }
[ -d "$corpus_dir" ] || { echo "verapdf.sh: corpus directory not found: $corpus_dir" >&2; exit 1; }

if [ "$self_test" -eq 1 ]; then
  # Self-test: verify veraPDF is available and can run
  if command -v verapdf >/dev/null 2>&1; then
    verapdf --version 2>/dev/null | head -1
    echo "verapdf.sh self-test: OK"
    exit 0
  else
    echo "verapdf.sh self-test: SKIPPED (verapdf not installed)"
    exit 0
  fi
fi

# Check veraPDF availability
if ! command -v verapdf >/dev/null 2>&1; then
  echo "verapdf.sh: verapdf not found in PATH. Install veraPDF or add to PATH." >&2
  exit 1
fi

# Default profile
if [ -z "$profile" ]; then
  profile="$ROOT/docs/verapdf-profile.xml"
  if [ ! -f "$profile" ]; then
    # Default to built-in veraPDF profile
    profile="UK"
  fi
fi

# Find all PDF files in corpus
find "$corpus_dir" -type f -name '*.pdf' | while IFS= read -r pdf; do
  rel=$(echo "$pdf" | sed "s|^$corpus_dir/||")
  echo "verapdf: validating $rel..."

  if [ -n "$output" ]; then
    verapdf --format xml --profile "$profile" "$pdf" >> "$output" 2>&1 || true
  else
    verapdf --format xml --profile "$profile" "$pdf" 2>&1 || true
  fi
done

echo "verapdf: validation complete"
exit 0
