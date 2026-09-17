#!/usr/bin/env bash
set -euo pipefail

# deps-refresh.sh (planned, PR #4)
# Refresh the Conan lockfile and run the full test/conformance suite before merge.
# Usage: ./tools/deps-refresh.sh
# This script is intentionally minimal for PR #4; the full implementation
# will:
#   1. Run `conan lock create` against the current conanfile.py
#   2. Run `ctest --preset ...` and `tools/check.sh`
#   3. Only exit 0 if all checks pass
echo "deps-refresh.sh: not yet implemented (planned, PR #4)"
exit 0