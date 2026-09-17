#!/bin/sh
# Verifies that the tree's text files and every sidecar fixture are byte-stable.
# Why this exists: ADR-0007 makes byte layout normative, and ADR-0010 puts the working copy in
# WSL2 while editors on the Windows side can silently rewrite line endings. A CRLF or a trailing
# space in a fixture is a bug in the repository, not a formatting preference.
#
# The byte tests are done with awk, not grep: a grep bracket like [ \t] also matches backslash and
# the letter t, which on the first run of this script accused 26 clean files of trailing whitespace.
set -u
cd "$(dirname "$0")/.." || exit 2

problems=0
files=0

list_files() {
  if git rev-parse --git-dir >/dev/null 2>&1; then
    git ls-files
  else
    find . -type f \
      -not -path './.git/*' -not -path './node_modules/*' \
      -not -path '*/__pycache__/*' -not -path './build*/*' | sed 's|^\./||'
  fi
}

for f in $(list_files | grep -E '\.(md|py|sh|yml|yaml|json|txt|cmake|in|env)$|editorconfig$|gitattributes$|gitignore$'); do
  [ -f "$f" ] || continue
  files=$((files + 1))
  case "$f" in tests/golden/*) continue ;; esac

  if head -c 3 "$f" | od -An -tx1 | tr -d ' \n' | grep -q '^efbbbf'; then
    printf '%s: BOM present\n' "$f"; problems=$((problems + 1))
  fi

  out=$(awk '
    BEGIN { cr = 0; ws = 0; tab = 0 }
    index($0, sprintf("%c", 13)) > 0 { cr = 1 }
    /[ \t]+$/ { ws = 1 }
    /^[ \t]*\t/ { tab = 1 }
    END {
      if (cr)  print "CR found (line endings must be LF; see .gitattributes)"
      if (ws)  print "trailing whitespace"
      if (tab && ENV["TAB_STRICT"] == "1") print "tab used for indentation"
    }' TAB_STRICT=0 "$f")
  case "$f" in
    *.py|*.yml|*.yaml|*.sh|*.json)
      tab=$(awk '/\t/{print "tab"; exit}' "$f")
      [ -z "$tab" ] || out="$out
tab character present (use spaces in this file type)"
      ;;
  esac
  if [ -n "$out" ]; then
    printf '%s\n' "$out" | while read -r line; do [ -n "$line" ] && printf '%s: %s\n' "$f" "$line"; done
    n=$(printf '%s\n' "$out" | grep -c .)
    problems=$((problems + n))
  fi
  if [ -s "$f" ] && [ "$(tail -c 1 "$f" | wc -l)" -eq 0 ]; then
    printf '%s: no final newline\n' "$f"; problems=$((problems + 1))
  fi
done

if ls tests/fixtures/sidecar/*.tynypdf.json >/dev/null 2>&1; then
  out=$(python3 tools/sidecar-fmt.py check tests/fixtures/sidecar 2>&1) || {
    printf '%s\n' "$out"; problems=$((problems + 1));
  }
  printf '%s\n' "$out" | grep -v '^sidecar-fmt: OK' | grep . || true
fi

if [ "$problems" -ne 0 ]; then
  printf 'canonical-check: %d problem(s) in %d file(s)\n' "$problems" "$files" >&2
  exit 1
fi
printf 'canonical-check: OK (%d text files, 0 canonical problems)\n' "$files"
