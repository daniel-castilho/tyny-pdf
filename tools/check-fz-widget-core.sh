#!/bin/bash
# R46.3 R47.2 - Check that fz_widget is not used in src/core (only allowed in backends/mupdf)
set -euo pipefail

if grep -rn "fz_widget" src/core 2>/dev/null | grep -v "\.git" | grep -v "third_party" | grep -v "binary"; then
    echo "ERROR: fz_widget found in src/core (should only be in backends/mupdf)"
    exit 1
fi

echo "OK: no fz_widget in src/core"
exit 0
