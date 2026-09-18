#!/bin/sh
# Runs every tool's own self-test, which is how this repository proves a check can still fail.
#
# Why this exists as a separate entry point: a gate that has only ever printed OK is an untested
# assertion (docs/lessons.md, and docs/testing-playbook.md section 3.5). tools/check.sh runs the
# checks; this runs the checks-on-the-checks. A tool is listed only when it ships a self-test; a
# tool without one (bench-measure.sh, canonical-check.sh) cannot claim that property
# and is deliberately absent.
#
# Usage: sh tools/gates-selftest.sh [--list]
set -u
cd "$(dirname "$0")/.." || exit 2

# name, command
SUITES="
lang-check          | python3 tools/lang-check.py --self-test
sidecar-fmt         | python3 tools/sidecar-fmt.py self-test
naming-sync         | python3 tools/naming-sync.py self-test
spec-check          | python3 tools/spec-check.py --self-test
docs-check          | python3 tools/docs-check.py --self-test
corpus-check        | python3 tools/corpus-check.py . --self-test
format-check        | sh tools/format-check.sh --self-test
layering-check      | sh tools/layering-check.sh --self-test
sbom                | sh tools/sbom.sh --self-test
deps-refresh        | sh tools/deps-refresh.sh --self-test
patch-report        | sh tools/patch-report.sh --self-test
verapdf             | sh tools/verapdf.sh --self-test
diff-scan           | python3 tools/diff-scan.py --self-test
"

if [ "${1:-}" = "--list" ]; then
  printf '%s\n' "$SUITES" | grep '|' | sed 's/ *|.*//;s/^ */  /'
  exit 0
fi

total=0
passed=0
failed=""
OLDIFS=$IFS
IFS='
'
for line in $SUITES; do
  [ -n "$line" ] || continue
  case "$line" in *\|*) : ;; *) continue ;; esac
  name=$(printf '%s' "$line" | cut -d'|' -f1 | tr -d ' ')
  cmd=$(printf '%s' "$line" | cut -d'|' -f2- | sed 's/^ *//')
  total=$((total + 1))
  printf '== %s: %s\n' "$name" "$cmd"
  if out=$(sh -c "$cmd" 2>&1); then
    passed=$((passed + 1))
    printf '   %s\n' "$(printf '%s\n' "$out" | tail -1)"
  else
    failed="$failed $name"
    printf '   FAIL\n%s\n' "$out"
  fi
done
IFS=$OLDIFS

printf '\n'
if [ "$total" -eq 0 ]; then
  printf 'gates-selftest: NOT A PASS - no suites were found\n' >&2
  exit 3
fi
if [ "$passed" -eq "$total" ]; then
  printf 'gates-selftest: OK (%s/%s suites hold)\n' "$passed" "$total"
  exit 0
fi
printf 'gates-selftest: %s/%s suites hold; failing:%s\n' "$passed" "$total" "$failed" >&2
exit 1
