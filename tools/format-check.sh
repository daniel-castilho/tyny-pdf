#!/usr/bin/env sh
# The format gate (docs/coding-standards.md sections 11-12, docs/dev-environment.md):
#
#   tools/format-check.sh                 # every tracked C/C++ file
#   tools/format-check.sh --self-test     # proves it can detect its own violation
#
# It is one of the checks --gates.yml runs and the CLI command behind the manual row of
# the AGENTS.md commands matrix. Any C/C++ file whose formatting differs from what
# clang-format 18.1.3 produces fails the build.
set -eu
cd "$(dirname "$0")/.."

if ! command -v clang-format >/dev/null 2>&1; then
  echo "format-check: clang-format not on PATH (install per docs/dev-environment.md)" >&2
  exit 2
fi
fmt=$(command -v clang-format)

if [ "${1:-}" = "--self-test" ]; then
  tmpdir=$(mktemp -d)
  trap 'rm -rf "$tmpdir"' EXIT
  printf 'int  x ;\n' >"$tmpdir/bad.cc"
  if "$fmt" --dry-run -Werror "$tmpdir/bad.cc" >/dev/null 2>&1; then
    echo "self-test FAIL: clang-format accepted 'int  x ;' (did not detect its own violation)" >&2
    exit 1
  fi
  printf 'int x;\n' >"$tmpdir/good.cc"
  if ! "$fmt" --dry-run -Werror "$tmpdir/good.cc" >/dev/null 2>&1; then
    echo "self-test FAIL: clang-format rejected already-formatted code" >&2
    exit 1
  fi
  echo "format-check self-test: 2/2 ok"
  exit 0
fi

# Only tracked code participates, and nothing under third_party/: vendored code keeps its
# upstream style (ADR-0004). git ls-files -z keeps this safe against odd filenames.
files=$(git ls-files -z | tr '\0' '\n' | grep -E '\.(c|cc|h|cpp)$' \
  | grep -v '^third_party/' || true)

if [ -z "$files" ]; then
  echo "format-check: nothing to check"
  exit 0
fi

# shellcheck disable=SC2086
exec "$fmt" --dry-run --Werror $files
