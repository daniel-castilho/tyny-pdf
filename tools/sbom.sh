#!/usr/bin/env bash
set -euo pipefail

# sbom.sh (planned, PR #4)
# Generates a CycloneDX SBOM from three signals:
#   1. The Conan lockfile (conan.lock)
#   2. The linker inputs actually consumed (-l, search paths, static archives)
#   3. A content fingerprint of the vendored source against known upstream releases
# Usage: ./tools/sbom.sh
# The SBOM is attached to the release; NOTICE lists engine, fonts, third parties.
echo "sbom.sh: not yet implemented (planned, PR #4)"
exit 0

