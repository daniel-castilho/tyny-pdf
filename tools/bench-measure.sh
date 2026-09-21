#!/usr/bin/env bash
# bench-measure.sh - M0.4 benchmark runner
# Usage: ./tools/bench-measure.sh [--target sumatra-3.6.1|sumatra-3.7pre|tynypdf] [--backend null|mupdf] [--corpus tests/bench/corpus] [--runs N] [--output results.json]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
HARNESS_DIR="${REPO_ROOT}/tests/bench/harness"
DEFAULT_CORPUS="${REPO_ROOT}/tests/bench/corpus"

TARGET=""
CORPUS="${DEFAULT_CORPUS}"
RUNS=1
OUTPUT_FILE=""
BACKEND="null"

usage() {
    cat <<EOF
Usage: $0 [options]
Options:
  --target TARGET       Target to benchmark: sumatra-3.6.1 | sumatra-3.7pre | tynypdf
  --backend BACKEND     tynypdf backend: null | mupdf (default: null)
  --corpus DIR          Corpus directory (default: ${DEFAULT_CORPUS})
  --runs N              Number of runs per metric (default: 1)
  --output FILE         Output JSON file (default: stdout)
  --help                Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --target) TARGET="$2"; shift 2 ;;
        --backend) BACKEND="$2"; shift 2 ;;
        --corpus) CORPUS="$2"; shift 2 ;;
        --runs) RUNS="$2"; shift 2 ;;
        --output) OUTPUT_FILE="$2"; shift 2 ;;
        --help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

if [[ -z "${TARGET}" ]]; then
    echo "Error: --target is required" >&2
    usage
    exit 1
fi

if [[ ! -d "${CORPUS}" ]]; then
    echo "Error: Corpus directory not found: ${CORPUS}" >&2
    exit 1
fi

# Verify harness exists
if [[ ! -f "${HARNESS_DIR}/run_benchmark.py" ]]; then
    echo "Error: Benchmark harness not found at ${HARNESS_DIR}/run_benchmark.py" >&2
    exit 1
fi

# Run the Python benchmark harness
python3 "${HARNESS_DIR}/run_benchmark.py" \
    --target "${TARGET}" \
    --backend "${BACKEND}" \
    --corpus "${CORPUS}" \
    --runs "${RUNS}" \
    ${OUTPUT_FILE:+--output "${OUTPUT_FILE}"}
