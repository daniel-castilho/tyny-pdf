#!/bin/sh
# Entry point for ADR-0005. The logic lives in the Python sibling so that the rule
# behaves identically on Windows developer machines and on Linux CI runners.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if command -v python3 >/dev/null 2>&1; then
  exec python3 "$SCRIPT_DIR/lang-check.py" "$@"
fi
if command -v python >/dev/null 2>&1; then
  exec python "$SCRIPT_DIR/lang-check.py" "$@"
fi
echo "lang-check: python3 is required (install it, or run tools/lang-check.py directly)" >&2
exit 2
