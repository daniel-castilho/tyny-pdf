#!/usr/bin/env bash
set -euo pipefail

# patch-report.sh (planned, PR #4)
# Prints the patch status table and fails the build when an entry is older than 180 days
# without a status change (decision D11).
# Usage: ./tools/patch-report.sh
# Reads patch headers from third_party/patches/*.patch and UPSTREAM.toml.
echo "patch-report.sh: not yet implemented (planned, PR #4)"
exit 0