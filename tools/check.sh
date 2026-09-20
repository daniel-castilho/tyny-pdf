#!/bin/sh
# Full repository gate, in the order a reviewer reads it. Usage: sh tools/check.sh
# Everything here is fast and offline; the corpus, sanitizer and fuzz jobs live in CI (ADR-0004).
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
self="$here/$(basename -- "$0")"
cd "$here/.." || exit 2

# Counted from this file, not from a path typed into it: a renamed or copied runner must still
# count itself. `|| true` keeps `grep -c` from printing 0 and failing at once, which would hand the
# guard below a two-line "0" that [ -eq 0 ] cannot even parse.
total=$(grep -c "^sec '" "$self" || true)
if [ "$total" -eq 0 ]; then
    printf 'check.sh: no sec() entries found in %s; refusing to run an empty gate\n' "$self" >&2
    exit 2
fi

i=0

sec() {
    i=$((i+1))
    printf '== %s/%s %s\n' "$i" "$total" "$1"
}

sec 'language (ADR-0005)'
python3 tools/lang-check.py
sec 'language self-test (the gate must still detect violations)'
python3 tools/lang-check.py --self-test >/dev/null && echo 'lang-check self-test: OK'

sec 'naming drift (ADR-0006 rules, ADR-0008 name)'
python3 tools/naming-sync.py check

sec 'sidecar format (ADR-0007)'
python3 tools/sidecar-fmt.py check tests/fixtures/sidecar
python3 tools/sidecar-fmt.py self-test >/dev/null && echo 'sidecar-fmt self-test: OK'

sec 'byte stability (.gitattributes, .editorconfig)'
sh tools/canonical-check.sh

sec 'living specs (R-M13)'
python3 tools/spec-check.py

sec 'C and C++ style against .clang-format (the tree has real C now)'
sh tools/format-check.sh
sh tools/format-check.sh --self-test >/dev/null && echo 'format-check self-test: OK'

sec 'diff and tree hygiene (D1-D8: exec bit, symlink, bidi, confusables, CI
events, action pins, dependency names, manifest and lock agreement)'
python3 tools/diff-scan.py --tree

sec 'diff-scan self-test (a scan that cannot fail is not a gate)'
python3 tools/diff-scan.py --self-test >/dev/null 2>&1 && echo 'diff-scan self-test: OK'

sec 'architecture layering (ADR-0011 R-M10/R-M11)'
sh tools/layering-check.sh --strict
sh tools/layering-check.sh --self-test >/dev/null && echo 'layering-check self-test: OK'

sec 'supply chain: SBOM generation (ADR-0004)'
sh tools/sbom.sh --output /tmp/sbom.json

sec 'supply chain: dependency refresh (ADR-0004)'
sh tools/deps-refresh.sh --dry-run

sec 'supply chain: patch report (ADR-0004)'
sh tools/patch-report.sh --fail-on-stale

sec 'documentation only promises what exists'
python3 tools/docs-check.py

printf '\ncheck: all gates green\n'
