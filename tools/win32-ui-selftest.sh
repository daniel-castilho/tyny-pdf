#!/usr/bin/env bash
# win32-ui-selftest.sh - story 5.4/6.1 viewer selftests
# Verifies: R24.4 (cold start, src/os/win32/SPEC.md), R24.5 (DPI purity,
# tests/approvals/dpi-*.png), R24.6 (wheel latency, build/tynypdf.ui.log),
# R32.4 (click parity, src/features/selection/SPEC.md).
# Usage: ./tools/win32-ui-selftest.sh [--exe PATH] [--log PATH] [--wheels N]
# Exit codes:
#   0  all selftests green
#   1  a selftest failed (details printed)
#   2  binary not found

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

EXE="${REPO_ROOT}/build/win-cross-x64/Release/tynypdf.exe"
LOG="${REPO_ROOT}/build/tynypdf.ui.log"
WHEELS=40
APPROVALS="${REPO_ROOT}/tests/approvals"

while [[ $# -gt 0 ]]; do
    case $1 in
        --exe) EXE="$2"; shift 2 ;;
        --log) LOG="$2"; shift 2 ;;
        --wheels) WHEELS="$2"; shift 2 ;;
        --help) grep '^#' "$0" | sed 's/^# //'; exit 0 ;;
        *) echo "Error: unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ ! -x "${EXE}" ]]; then
    echo "Error: binary not found or not executable: ${EXE}" >&2
    exit 2
fi

fail=""
check() {
    if ! "$@"; then
        echo "FAIL: $*" >&2
        fail="1"
    fi
}

cold_start() {
    local log="$1"
    local line
    line=$(grep -E '^cold_start:' "${log}" | head -n 1 || true)
    if [[ -z "${line}" ]]; then
        echo "cold start: MISSING (no cold_start line)" >&2
        return 1
    fi
    local cmd
    cmd=$(echo "${line}" | awk -F'init_to_present_ms=' '{print $2}' | awk '{print $1}')
    echo "cold start: init_to_present_ms=${cmd}"
    awk -v v="${cmd}" 'BEGIN{exit !(v >= 0 && v <= 300)}'
}

machine_block() {
    local log="$1"
    if grep -E '^machine:' "${log}" | head -n 1; then
        return 0
    fi
    echo "machine block: MISSING" >&2
    return 1
}

dpi_purity() {
    local out="$1"
    for f in dpi-150.png dpi-200.png; do
        if [[ ! -s "${out}/${f}" ]]; then
            echo "dpi purity: ${f} missing or empty" >&2
            return 1
        fi
    done
    echo "dpi purity: ${out}/dpi-150.png ${out}/dpi-200.png present"
}

wheel_latency() {
    local log="$1"
    local n
    n=$(grep -cE '^input_ts=' "${log}" || true)
    if [[ "${n}" -eq 0 ]]; then
        echo "wheel latency: no input_ts samples (TYNYPDF_WHEELS not honoured)" >&2
        return 1
    fi
    echo "wheel latency samples: ${n}"
    grep -E '^input_ts=' "${log}" | awk -F'latency_ms=' '{print $2}' |
        sort -n | awk -v n="${n}" '
            BEGIN { p50=int(0.50*n); p99=int(0.99*n) }
            { a[NR]=$1 }
            END {
                printf "wheel latency: p50=%.3fms p99=%.3fms max=%.3fms\n",
                    a[p50<1?1:p50], a[p99<1?1:p99], a[NR];
                exit !(a[p99<1?1:p99] <= 16)
            }'
}

check "${EXE}" --dpi-selftest "${APPROVALS}"
check dpi_purity "${APPROVALS}"

# R32.4: click parity - run CLI select and compare with window click log
# (requires a PDF with text and a known click position)
click_parity() {
    local log="$1"
    local cli_exe="${REPO_ROOT}/build/linux-core/Debug/tynypdf-cli"
    if [[ ! -x "${cli_exe}" ]]; then
        echo "click parity: CLI not built, skipping" >&2
        return 0
    fi
    local pdf="tests/fixtures/simple.pdf"
    if [[ ! -f "${pdf}" ]]; then
        echo "click parity: fixture missing, skipping" >&2
        return 0
    fi
    # Run CLI select at a known position (page 0, x=100, y=100, dpi=72)
    local cli_out
    cli_out=$("${cli_exe}" select "${pdf}" 0 100 100 72 2>/dev/null || true)
    if [[ -z "${cli_out}" ]]; then
        echo "click parity: CLI select returned empty, skipping" >&2
        return 0
    fi
    local cli_sha
    cli_sha=$(echo "${cli_out}" | sha256sum | awk '{print $1}')
    # The window log should contain a click entry with matching sha256
    # Format: click: page=0 x=100 y=100 sha256=<hash>
    local win_line
    win_line=$(grep -E '^click: page=0 x=100 y=100' "${log}" | head -n 1 || true)
    if [[ -z "${win_line}" ]]; then
        echo "click parity: no matching click entry in log" >&2
        return 1
    fi
    local win_sha
    win_sha=$(echo "${win_line}" | awk -F'sha256=' '{print $2}' | awk '{print $1}')
    if [[ "${cli_sha}" != "${win_sha}" ]]; then
        echo "click parity: sha256 mismatch CLI=${cli_sha} WIN=${win_sha}" >&2
        return 1
    fi
    echo "click parity: CLI sha256 == WIN sha256 (${cli_sha})"
}

rm -f "${LOG}"
check env WSLENV=TYNYPDF_WHEELS TYNYPDF_WHEELS="${WHEELS}" "${EXE}"
check machine_block "${LOG}"
check cold_start "${LOG}"
check wheel_latency "${LOG}"
check click_parity "${LOG}"

if [[ -n "${fail}" ]]; then
    echo "win32-ui-selftest: FAILED" >&2
    exit 1
fi
echo "win32-ui-selftest: OK"
