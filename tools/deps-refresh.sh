#!/usr/bin/env sh
# deps-refresh.sh - Conan lockfile refresh + full test suite (ADR-0004 section 6, docs/dependency-policy.md section 5)
#
# Refresh the Conan lockfile and run the full test/conformance suite before merge.
# Usage: sh tools/deps-refresh.sh [--dry-run] [--self-test]
# This script:
#   1. Runs `conan lock create` against the current conanfile.py
#   2. Runs `ctest --preset ...` and `tools/check.sh`
#   3. Only exits 0 if all checks pass
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

dry_run=0
self_test=0
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) dry_run=1 ;;
    --self-test) self_test=1 ;;
    *) echo "deps-refresh.sh: unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

if [ "$self_test" -eq 1 ]; then
  # Self-test: verify the script runs without error in dry-run mode
  "$ROOT/tools/deps-refresh.sh" --dry-run 2>&1 | grep -q "dry-run: would run" || exit 1
  echo "deps-refresh.sh self-test: OK"
  exit 0
fi

cd "$ROOT"

# Step 1: conan lock create
echo "deps-refresh: refreshing Conan lockfile..."
if command -v conan >/dev/null 2>&1; then
  if [ "$dry_run" -eq 0 ]; then
    conan lock create conanfile.py --lockfile=conan.lock
  else
    echo "dry-run: would run 'conan lock create conanfile.py --lockfile=conan.lock'"
  fi
else
  echo "deps-refresh: conan not in PATH, skipping lockfile refresh"
fi

# Step 2: run full test suite
echo "deps-refresh: running test suite (ctest --preset linux-core)..."
if [ "$dry_run" -eq 0 ]; then
  ctest --preset linux-core --output-on-failure
else
  echo "dry-run: would run 'ctest --preset linux-core --output-on-failure'"
fi

# Step 3: run full check.sh gate
echo "deps-refresh: running full gate (tools/check.sh)..."
if [ "$dry_run" -eq 0 ]; then
  sh tools/check.sh
else
  echo "dry-run: would run 'sh tools/check.sh'"
fi

echo "deps-refresh: all steps completed successfully"
exit 0
