#!/usr/bin/env bash
set -euo pipefail

# Empty-tree behaviour: until story 1.4 lands, a green run over a missing directory is a failure.
if [ ! -d src/core ]; then
    echo "layering-check: src/core does not exist (expected before story 1.4); this is the designed failure mode"
    exit 1
fi

# Approximate line counts for the ADR-0011 R-M6 ratio:
#   backend_line_ratio = lines in src/backends/mupdf / (lines in src/ + include/)
# We count only the files we know exist; other files are ignored for now.
mupdf_lines=$(find src/backends/mupdf -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)
total_lines=$(find src -path '*/backends/*' -prune -o -name '*.c' -o -name '*.h' -o -name '*.cc' -o -name '*.cpp' | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo 0)

if [ -z "$mupdf_lines" ] || [ -z "$total_lines" ] || [ "$total_lines" -eq 0 ]; then
    echo "layering-check: could not count lines"
    exit 1
fi

ratio=$(echo "scale=4; $mupdf_lines / $total_lines" | bc)
ratio_rounded=$(printf "%.4f" "$ratio")

# ADR-0011 R-M6: fail if ratio > 0.15
if [ $(echo "$ratio_rounded > 0.15" | bc) -eq 1 ]; then
    echo "layering-check: ratio $ratio_rounded exceeds 0.15 (ADR-0011 R-M6)"
    exit 1
fi

echo "layering-check: ratio $ratio_rounded (under 0.15, ADR-0011 R-M6 satisfied)"
exit 0