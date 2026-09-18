#!/usr/bin/env sh
# patch-report.sh - Patch status table + 180-day expiry (ADR-0004 section 6, docs/dependency-policy.md section 6)
#
# Prints the patch status table and fails the build when an entry is older than 180 days
# without a status change (decision D11).
# Usage: sh tools/patch-report.sh [--fail-on-stale] [--self-test]
# Reads patch headers from third_party/patches/*.patch and UPSTREAM.toml.
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

fail_on_stale=0
self_test=0
while [ $# -gt 0 ]; do
  case "$1" in
    --fail-on-stale) fail_on_stale=1 ;;
    --self-test) self_test=1 ;;
    *) echo "patch-report.sh: unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done

if [ "$self_test" -eq 1 ]; then
  # Self-test: create a temp dir with a stale patch and verify detection
  base=$(mktemp -d)
  trap 'rm -rf "$base"' EXIT
  mkdir -p "$base/patches"
  # Create a patch older than 180 days with status "none"
  old_date=$(date -d "200 days ago" +%Y-%m-%d 2>/dev/null || date -v-200d +%Y-%m-%d)
  cat > "$base/patches/stale.patch" <<EOF
# Subject: Stale patch
# Reason: Test
# Upstream-status: none
# Owner: test
# Date: $old_date
EOF
  # Run patch-report on temp dir
  PATCH_DIR="$base/patches" "$ROOT/tools/patch-report.sh" --fail-on-stale >/dev/null 2>&1 && exit 1 || exit 0
fi

cd "$ROOT"

PATCH_DIR="${PATCH_DIR:-third_party/patches}"

if [ ! -d "$PATCH_DIR" ]; then
  echo "patch-report: no patch directory $PATCH_DIR"
  exit 0
fi

now=$(date +%s)
stale_count=0

echo "Patch Status Report"
echo "==================="
printf "%-30s %-12s %-10s %-10s %s\n" "PATCH" "STATUS" "AGE (days)" "OWNER" "SUBJECT"
echo "--------------------------------------------------------------------------------"

for patch in "$PATCH_DIR"/*.patch; do
  [ -e "$patch" ] || continue
  base=$(basename "$patch")

  # Extract headers
  subject=$(sed -n 's/^# Subject: //p' "$patch" | head -1)
  reason=$(sed -n 's/^# Reason: //p' "$patch" | head -1)
  status=$(sed -n 's/^# Upstream-status: //p' "$patch" | head -1)
  owner=$(sed -n 's/^# Owner: //p' "$patch" | head -1)
  date_str=$(sed -n 's/^# Date: //p' "$patch" | head -1)

  [ -n "$subject" ] || subject="(no subject)"
  [ -n "$reason" ] || reason="(no reason)"
  [ -n "$status" ] || status="none"
  [ -n "$owner" ] || owner="(unknown)"

  # Compute age if Date header exists
  age_days=""
  if [ -n "$date_str" ]; then
    # Try parsing date in multiple formats
    date_epoch=$(date -d "$date_str" +%s 2>/dev/null || date -j -f "%Y-%m-%d" "$date_str" +%s 2>/dev/null || echo "")
    if [ -n "$date_epoch" ]; then
      age_days=$(( (now - date_epoch) / 86400 ))
    fi
  fi

  # Fallback to file mtime if no Date header
  if [ -z "$age_days" ]; then
    mtime=$(stat -c %Y "$patch" 2>/dev/null || stat -f %m "$patch" 2>/dev/null)
    if [ -n "$mtime" ]; then
      age_days=$(( (now - mtime) / 86400 ))
    fi
  fi

  [ -n "$age_days" ] || age_days="?"

  printf "%-30s %-12s %-10s %-10s %s\n" "$base" "$status" "$age_days" "$owner" "$subject"

  # Check staleness: status "none" and age > 180 days
  if [ "$status" = "none" ] && [ "$age_days" != "?" ] && [ "$age_days" -gt 180 ]; then
    stale_count=$((stale_count + 1))
    echo "  >> STALE: $base has status 'none' for $age_days days (>180)"
  fi
done

echo ""
echo "Summary: $stale_count stale patch(es)"

if [ "$fail_on_stale" -eq 1 ] && [ "$stale_count" -gt 0 ]; then
  echo "patch-report: FAIL -- $stale_count stale patch entry(ies) exceed 180 days"
  exit 1
fi

exit 0
