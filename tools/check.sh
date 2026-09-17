#!/bin/sh
# Full repository gate, in the order a reviewer reads it. Usage: sh tools/check.sh
# Everything here is fast and offline; the corpus, sanitizer and fuzz jobs live in CI (ADR-0004).
set -eu
cd "$(dirname "$0")/.." || exit 2

printf '== 1/7 language (ADR-0005)\n'
python3 tools/lang-check.py
printf '== 2/7 language self-test (the gate must still detect violations)\n'
python3 tools/lang-check.py --self-test >/dev/null && echo 'lang-check self-test: OK'

printf '== 3/7 naming drift (ADR-0006 rules, ADR-0008 name)\n'
python3 tools/naming-sync.py check

printf '== 4/7 sidecar format (ADR-0007)\n'
python3 tools/sidecar-fmt.py check tests/fixtures/sidecar
python3 tools/sidecar-fmt.py self-test >/dev/null && echo 'sidecar-fmt self-test: OK'

printf '== 5/7 byte stability (.gitattributes, .editorconfig)\n'
sh tools/canonical-check.sh

printf '== 6/7 living specs (R-M13)\n'
python3 tools/spec-check.py

printf '== 7/7 documentation only promises what exists\n'
python3 tools/docs-check.py

printf '\ncheck: all gates green\n'
