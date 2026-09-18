#!/usr/bin/env sh
# The architecture boundary gate (ADR-0011 R-M10 and R-M11, ADR-0002).
#
#   sh tools/layering-check.sh               # report; fail on a violation or an over-budget ratio
#   sh tools/layering-check.sh --strict      # also fail when the ratio is undefined
#   sh tools/layering-check.sh --self-test   # prove the gate can detect its own violations
#   sh tools/layering-check.sh --root DIR    # inspect another tree (used by --self-test)
#
# What it enforces, read off the tree, not argued:
#
#   R-M10  nothing outside src/backends/** includes an engine header; src/core/** and
#          src/render/** include no Windows or DirectX header; src/render/** and src/os/** reference
#          no engine symbol (fz_, pdfium_) and no IR mutation
#          (pc_txn_*, pc_redact_*, pc_annot_{add,remove,update,set,insert,delete}).
#   R-M11  backend_line_ratio = lines under src/backends/** that are not inside a function assigned
#          into that backend's pc_backend_api vtable (the vtable implementation, i.e. the code a
#          backend has to carry anyway) divided by all lines under src/** and include/**; fails
#          above 0.15. ADR-0011's correction of 2026-09-17 fixes that as the operational formula.
#
# Empty-tree behaviour: a tree with no C/C++ file has an undefined ratio. That is never a pass:
# the line prints its reason and --strict fails on it (ADR-0011 R-M6/R-M11).
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

strict=0
self_test=0
root=
while [ $# -gt 0 ]; do
  case "$1" in
    --strict) strict=1 ;;
    --self-test) self_test=1 ;;
    --root) shift; root=${1:-} ;;
    *) echo "layering-check: unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done
[ -n "$root" ] || root=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

# find_sources <dir>: every counted C/C++ file under <dir>, one per line.
find_sources() {
  find "$1" -type f \
    \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) 2>/dev/null \
    || true
}

# vtable_names <file>: the function names assigned in that file's pc_backend_api initializer.
vtable_names() {
  sed -n '/pc_backend_api[^;]*= *{/,/^};/p' "$1" 2>/dev/null \
    | sed -n 's/.*\.\([a-zA-Z_][a-zA-Z0-9_]*\) *= *\([a-zA-Z_][a-zA-Z0-9_]*\)[[:space:]]*,.*/\2/p' \
    | tr '\n' ' '
}

# outside_lines <file> <names>: lines not inside a function body named in <names>. A function body
# runs from its definition line to the matching closing brace; a declaration ending in ';' starts
# nothing, and a brace is counted when it appears on the definition or the line after it.
outside_lines() {
  awk -v names="$2" '
    BEGIN {
      n = split(names, a, " ")
      for (i = 1; i <= n; i++) if (a[i] != "") fn[a[i]] = 1
      inbody = 0; depth = 0; seen = 0; outside = 0
    }
    { lines[NR] = $0 }
    END {
      for (i = 1; i <= NR; i++) {
        line = lines[i]; nextline = lines[i + 1]
        if (!inbody) {
          started = 0
          if (line !~ /;[[:space:]]*$/) {
            for (name in fn)
              if (line ~ /\(/ && line ~ ("(^|[^A-Za-z0-9_])" name "([^A-Za-z0-9_]|$)") \
                  && (line ~ /\{/ || nextline ~ /\{/)) { started = 1; break }
          }
          if (!started) { outside++; continue }
          inbody = 1; depth = 0; seen = 0
        }
        nl = gsub(/\{/, "{", line); nr = gsub(/\}/, "}", line)
        depth += nl - nr
        if (nl > 0) seen = 1
        if (seen && depth <= 0) inbody = 0
      }
      print outside
    }
  ' "$1"
}

# scan <root> <violations-file>: append one line per violation, echo "files=N num=N den=N".
scan() {
  scan_root=$1
  scan_viol=$2
  scan_files=0
  scan_num=0
  scan_den=0
  scan_list=$(find_sources "$scan_root/src"; find_sources "$scan_root/include")
  OLDIFS=$IFS
  IFS='
'
  for f in $scan_list; do
    [ -n "$f" ] || continue
    scan_files=$((scan_files + 1))
    lines=$(wc -l <"$f" 2>/dev/null | tr -d '[:space:]')
    [ -n "$lines" ] || lines=0
    scan_den=$((scan_den + lines))
    rel=${f#"$scan_root"/}
    case "$rel" in
      src/backends/*)
        names=$(vtable_names "$f")
        if [ -n "$names" ]; then
          outside=$(outside_lines "$f" "$names")
          scan_num=$((scan_num + outside))
        else
          scan_num=$((scan_num + lines))
        fi
        ;;
      *)
        if grep -Eq '#include[[:space:]]*[<"](fitz|mupdf|pdfium)' "$f" 2>/dev/null; then
          printf 'R-M2/R-M4 engine header outside src/backends/: %s\n' "$rel" >>"$scan_viol"
        fi
        ;;
    esac
    case "$rel" in
      src/core/*|src/render/*)
        if grep -Eq '#include[[:space:]]*[<"](windows|windef|unknwn|objbase|d2d1|dwrite|d3d11|dxgi)' \
          "$f" 2>/dev/null; then
          printf 'R-M10 Windows/DirectX header in %s\n' "$rel" >>"$scan_viol"
        fi
        ;;
    esac
    case "$rel" in
      src/render/*|src/os/*)
        if grep -Eq '\b(fz_[a-z]|pdfium_[a-z])' "$f" 2>/dev/null; then
          printf 'R-M10 engine symbol in %s\n' "$rel" >>"$scan_viol"
        fi
        if grep -Eq '\bpc_(txn_[a-z]|redact_[a-z]|annot_(add|remove|update|set|insert|delete))' \
          "$f" 2>/dev/null; then
          printf 'R-M10 IR mutation in %s\n' "$rel" >>"$scan_viol"
        fi
        ;;
    esac
  done
  IFS=$OLDIFS
  printf 'files=%s num=%s den=%s\n' "$scan_files" "$scan_num" "$scan_den"
}

# evaluate <root> <strict>: print the verdict and return 0 (pass) or 1 (fail).
evaluate() {
  eval_root=$1
  eval_strict=$2
  eval_viol=$(mktemp)
  eval_summary=$(scan "$eval_root" "$eval_viol")
  eval_files=$(printf '%s' "$eval_summary" | sed 's/.*files=\([0-9]*\).*/\1/')
  eval_num=$(printf '%s' "$eval_summary" | sed 's/.*num=\([0-9]*\).*/\1/')
  eval_den=$(printf '%s' "$eval_summary" | sed 's/.*den=\([0-9]*\).*/\1/')
  eval_count=$(wc -l <"$eval_viol" | tr -d '[:space:]')
  [ -n "$eval_count" ] || eval_count=0

  if [ "$eval_den" -eq 0 ]; then
    eval_ratio='undefined (no C/C++ file under src/ or include/)'
    eval_over=1
  else
    eval_ratio=$(awk -v n="$eval_num" -v d="$eval_den" 'BEGIN { printf "%.4f", n / d }')
    if awk -v n="$eval_num" -v d="$eval_den" 'BEGIN { exit !(n / d > 0.15) }'; then
      eval_over=1
    else
      eval_over=0
    fi
  fi
  printf 'layering-check: backend_line_ratio=%s\n' "$eval_ratio"

  eval_fail=0
  [ "$eval_count" -gt 0 ] && eval_fail=1
  if [ "$eval_den" -eq 0 ]; then
    [ "$eval_strict" -eq 1 ] && eval_fail=1
  elif [ "$eval_over" -eq 1 ]; then
    eval_fail=1
  fi

  if [ "$eval_fail" -eq 0 ]; then
    printf 'layering-check: OK (%s source files, %s violations)\n' "$eval_files" "$eval_count"
    rm -f "$eval_viol"
    return 0
  fi
  if [ "$eval_count" -gt 0 ]; then
    while IFS= read -r eval_line; do
      printf 'layering-check: %s\n' "$eval_line"
    done <"$eval_viol"
  fi
  if [ "$eval_den" -eq 0 ]; then
    printf 'layering-check: %s violation(s), ratio over budget or undefined under --strict\n' \
      "$eval_count"
  else
    printf 'layering-check: %s violation(s), backend_line_ratio %s exceeds 0.15 (ADR-0011 R-M11)\n' \
      "$eval_count" "$eval_ratio"
  fi
  rm -f "$eval_viol"
  return 1
}

if [ "$self_test" -eq 1 ]; then
  base=$(mktemp -d)
  trap 'rm -rf "$base"' EXIT
  ok=0
  total=0

  # clean: a vtable function, plus enough core code that the ratio stays under budget.
  mkdir -p "$base/clean/src/core" "$base/clean/src/backends/null"
  i=1
  while [ "$i" -le 40 ]; do
    printf 'int core_%s;\n' "$i"
    i=$((i + 1))
  done >"$base/clean/src/core/a.cc"
  {
    printf '#include "pdfcore/backend.h"\n'
    printf 'static int null_open(void) {\n  return 0;\n}\n'
    printf 'pc_backend_api api = {\n  .doc_open = null_open,\n};\n'
  } >"$base/clean/src/backends/null/n.cc"
  total=$((total + 1))
  if evaluate "$base/clean" 0 >/dev/null 2>&1; then
    ok=$((ok + 1))
  else
    printf 'self-test FAIL: a clean tree was rejected\n' >&2
  fi

  mkdir -p "$base/win_core/src/core"
  printf '#include <windows.h>\nint a;\n' >"$base/win_core/src/core/a.cc"
  total=$((total + 1))
  if evaluate "$base/win_core" 0 >/dev/null 2>&1; then
    printf 'self-test FAIL: a Windows header in src/core did not fail the gate\n' >&2
  else
    ok=$((ok + 1))
  fi

  mkdir -p "$base/d2d_render/src/render"
  printf '#include <d2d1.h>\nint a;\n' >"$base/d2d_render/src/render/a.cc"
  total=$((total + 1))
  if evaluate "$base/d2d_render" 0 >/dev/null 2>&1; then
    printf 'self-test FAIL: a Direct2D header in src/render did not fail the gate\n' >&2
  else
    ok=$((ok + 1))
  fi

  mkdir -p "$base/engine_cli/src/cli"
  printf '#include <mupdf/fitz.h>\nint a;\n' >"$base/engine_cli/src/cli/a.cc"
  total=$((total + 1))
  if evaluate "$base/engine_cli" 0 >/dev/null 2>&1; then
    printf 'self-test FAIL: an engine header outside a backend did not fail the gate\n' >&2
  else
    ok=$((ok + 1))
  fi

  mkdir -p "$base/engine_render/src/render"
  printf 'void f(void) { fz_matrix m; (void)m; }\n' >"$base/engine_render/src/render/a.cc"
  total=$((total + 1))
  if evaluate "$base/engine_render" 0 >/dev/null 2>&1; then
    printf 'self-test FAIL: an engine symbol in src/render did not fail the gate\n' >&2
  else
    ok=$((ok + 1))
  fi

  mkdir -p "$base/mutate_os/src/os/win32"
  printf 'void f(void) { pc_txn_begin(); }\n' >"$base/mutate_os/src/os/win32/a.cc"
  total=$((total + 1))
  if evaluate "$base/mutate_os" 0 >/dev/null 2>&1; then
    printf 'self-test FAIL: IR mutation in src/os did not fail the gate\n' >&2
  else
    ok=$((ok + 1))
  fi

  # over_budget: the vtable function is small, but a thick body of non-vtable backend code is not.
  mkdir -p "$base/over_budget/src/core" "$base/over_budget/src/backends/mupdf"
  printf 'int core_1;\n' >"$base/over_budget/src/core/a.cc"
  {
    printf 'static int e(void) {\n  return 0;\n}\n'
    i=1
    while [ "$i" -le 95 ]; do
      printf 'int g%s;\n' "$i"
      i=$((i + 1))
    done
    printf 'pc_backend_api api = {\n  .doc_open = e,\n};\n'
  } >"$base/over_budget/src/backends/mupdf/big.cc"
  total=$((total + 1))
  if evaluate "$base/over_budget" 0 >/dev/null 2>&1; then
    printf 'self-test FAIL: backend_line_ratio above 0.15 did not fail the gate\n' >&2
  else
    ok=$((ok + 1))
  fi

  mkdir -p "$base/empty/src/core"
  total=$((total + 1))
  if evaluate "$base/empty" 0 >/dev/null 2>&1 && ! evaluate "$base/empty" 1 >/dev/null 2>&1; then
    ok=$((ok + 1))
  else
    printf 'self-test FAIL: an undefined ratio must pass by default and fail under --strict\n' >&2
  fi

  if [ "$ok" -eq "$total" ]; then
    printf 'layering-check self-test: %s/%s ok\n' "$ok" "$total"
    exit 0
  fi
  printf 'layering-check self-test: %s/%s ok\n' "$ok" "$total" >&2
  exit 1
fi

evaluate "$root" "$strict"
