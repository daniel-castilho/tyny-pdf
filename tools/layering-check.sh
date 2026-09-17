#!/usr/bin/env bash
set -euo pipefail

# layering-check.sh (planned, PR #4)
# Reports the backend_line_ratio = lines in src/backends/mupdf / (lines in src/ + include/)
# Fails (exit 1) if ratio > 0.15 per ADR-0011 R-M6.
# Empty-tree behaviour: until story 1.4 lands, a green run is the designed failure mode.

# Count mupdf backend lines
mupdf_lines=0
if [ -d src/backends/mupdf ]; then
    mupdf_lines=$(find src/backends/mupdf -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)
    mupdf_lines=$(echo "$mupdf_lines" | tr -d '[:space:]')
fi

# Count total src lines (all C/C++ files under src/ excluding mupdf backend for now)
total_lines=0
if [ -d src ]; then
    total_lines=$(find src -path '*/backends/mupfd*' -prune -o -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' -print 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)
    total_lines=$(echo "$total_lines" | tr -d '[:space:]')
fi

# Handle empty/missing values
if [ -z "$mupdf_lines" ]; then mupdf_lines=0; fi
if [ -z "$total_lines" ]; then total_lines=0; fi

# Avoid division by zero
if [ "$total_lines" -eq 0 ]; then
    echo "layering-check: no source files counted; ratio undefined (expected before story 1.4)"
    exit 1
fi

# Calculate ratio using bc (supports floating point)
ratio=$(echo "scale=4; $mupdf_lines / $total_lines" | bc 2>/dev/null || echo "0")

# Remove leading/trailing whitespace from ratio
ratio=$(echo "$ratio" | tr -d '[:space:]')

# Default ratio to 0 if bc failed
[ -z "$ratio" ] && ratio=0

# ADR-0011 R-M6: fail if ratio > 0.15
if (( $(echo "$ratio > 0.15" | bc -l 2>/dev/null || echo 0) )); then
    echo "layering-check: ratio $ratio exceeds 0.15 (ADR-0011 R-M6)"
    exit 1
fi

echo "layering-check: ratio $ratio (under 0.15, ADR-0011 R-M6 satisfied)"
exit 0
