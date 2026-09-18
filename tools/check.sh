#!/bin/sh
# Full repository gate, in the order a reviewer reads it. Usage: sh tools/check.sh
# Everything here is fast and offline; the corpus, sanitizer and fuzz jobs live in CI (ADR-0004).
set -eu
cd "$(dirname "$0")/.." || exit 2

printf '== 1/10 language (ADR-0005)\n'
python3 tools/lang-check.py
printf '== 2/10 language self-test (the gate must still detect violations)\n'
python3 tools/lang-check.py --self-test >/dev/null && echo 'lang-check self-test: OK'

printf '== 3/10 naming drift (ADR-0006 rules, ADR-0008 name)\n'
python3 tools/naming-sync.py check

printf '== 4/10 sidecar format (ADR-0007)\n'
python3 tools/sidecar-fmt.py check tests/fixtures/sidecar
python3 tools/sidecar-fmt.py self-test >/dev/null && echo 'sidecar-fmt self-test: OK'

printf '== 5/10 byte stability (.gitattributes, .editorconfig)\n'
sh tools/canonical-check.sh

printf '== 6/10 living specs (R-M13)\n'
python3 tools/spec-check.py

printf '== 7/10 C and C++ style against .clang-format (the tree has real C now)\n'
sh tools/format-check.sh
sh tools/format-check.sh --self-test >/dev/null && echo 'format-check self-test: OK'

printf '== 8/10 diff and tree hygiene (D1-D8: exec bit, symlink, bidi, confusables, CI\n   events, action pins, dependency names, manifest and lock agreement)\n'
python3 tools/diff-scan.py --tree

printf '== 9/10 diff-scan self-test (a scan that cannot fail is not a gate)\n'
python3 tools/diff-scan.py --self-test >/dev/null 2>&1 && echo 'diff-scan self-test: OK'

printf '== 10/10 documentation only promises what exists\n'
python3 tools/docs-check.py

printf '\ncheck: all gates green\n'
