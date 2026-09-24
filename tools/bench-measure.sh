#!/usr/bin/env bash
# bench-measure.sh - M0.4 / Epic 5 benchmark runner
# Verifies: R15.1 (src/features/render/SPEC.md) - the frame-budget measurement harness;
# story 1.5: R30.2 (src/app/viewer/SPEC.md) - the viewer's 250 MiB resident-memory ceiling
# on a 1000-page corpus at three tiles per page;
# R31.1 (src/app/bench/SPEC.md) - the viewer bench mode that emits frame_ms/peak_rss_kib.
# Usage: ./tools/bench-measure.sh [--target sumatra-3.6.1|sumatra-3.7pre|tynypdf]
#                          [--backend null|mupdf] [--corpus tests/bench/corpus]
#                          [--runs N] [--output results.json]
#                          [--binary PATH] [--record-machine] [--compare FILE1 FILE2] [--machine SPEC]
#                          [--bench viewer] [--tiles N] [--rows N]

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
BINARY=""
RECORD_MACHINE=false
COMPARE_MODE=false
COMPARE_FILE1=""
COMPARE_FILE2=""
MACHINE_OVERRIDE=""
BENCH=""
BENCH_TILES=""
BENCH_ROWS=""

usage() {
    cat <<EOF
Usage: $0 [options]
Options:
  --target TARGET       Target to benchmark: sumatra-3.6.1 | sumatra-3.7pre | tynypdf
  --backend BACKEND     tynypdf backend: null | mupdf (default: null)
  --corpus DIR          Corpus directory (default: ${DEFAULT_CORPUS})
  --runs N              Number of runs per metric (default: 1)
  --output FILE         Output JSON file (default: stdout)
  --binary PATH         Path to tynypdf.exe (required for tynypdf target on Windows)
  --record-machine      Include machine_spec in output JSON
  --compare FILE1 FILE2 Compare two JSON runs (exit 0 if within 10% and the 0.5-unit noise floor, 1 otherwise)
  --machine SPEC        Override machine_spec for cross-machine compare (values: 'other')
  --bench viewer        Viewer bench mode (story 1.5): tynypdf.exe --bench over the corpus
  --tiles N             Viewer bench strip width in tiles (default: 3)
  --rows N              Viewer bench strip height in tile rows (default: 1)
  --help                Show this help

Exit codes (for refusal tests):
  0  Success
  1  General error
  2  Missing binary (--binary not found or not executable)
  3  Non-PDF in corpus (no .pdf files found)
  4  Cross-machine compare failed (machine_spec mismatch)
EOF
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --target) TARGET="$2"; shift 2 ;;
        --backend) BACKEND="$2"; shift 2 ;;
        --corpus) CORPUS="$2"; shift 2 ;;
        --runs) RUNS="$2"; shift 2 ;;
        --output) OUTPUT_FILE="$2"; shift 2 ;;
        --binary) BINARY="$2"; shift 2 ;;
        --record-machine) RECORD_MACHINE=true; shift ;;
        --compare) COMPARE_MODE=true; COMPARE_FILE1="$2"; COMPARE_FILE2="$3"; shift 3 ;;
        --machine) MACHINE_OVERRIDE="$2"; shift 2 ;;
        --bench) BENCH="$2"; shift 2 ;;
        --tiles) BENCH_TILES="$2"; shift 2 ;;
        --rows) BENCH_ROWS="$2"; shift 2 ;;
        --help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

# Compare mode: exit early with appropriate code
if [[ "${COMPARE_MODE}" == "true" ]]; then
    python3 "${HARNESS_DIR}/run_benchmark.py" --compare "${COMPARE_FILE1}" "${COMPARE_FILE2}" ${MACHINE_OVERRIDE:+--machine "${MACHINE_OVERRIDE}"}
    exit $?
fi

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

# Validate binary for tynypdf target
if [[ "${TARGET}" == "tynypdf" ]]; then
    if [[ -z "${BINARY}" ]]; then
        echo "Error: --binary is required for tynypdf target" >&2
        exit 2
    fi
    if [[ ! -f "${BINARY}" || ! -x "${BINARY}" ]]; then
        echo "Error: Binary not found or not executable: ${BINARY}" >&2
        exit 2
    fi
fi

# Check for PDF files in corpus
PDF_COUNT=$(find "${CORPUS}" -name "*.pdf" -type f 2>/dev/null | wc -l)
if [[ "${PDF_COUNT}" -eq 0 ]]; then
    echo "Error: No PDF files found in corpus: ${CORPUS}" >&2
    exit 3
fi

# Run the Python benchmark harness
python3 "${HARNESS_DIR}/run_benchmark.py" \
    --target "${TARGET}" \
    --backend "${BACKEND}" \
    --corpus "${CORPUS}" \
    --runs "${RUNS}" \
    ${BINARY:+--binary "${BINARY}"} \
    ${RECORD_MACHINE:+--record-machine} \
    ${BENCH:+--bench "${BENCH}"} \
    ${BENCH_TILES:+--tiles "${BENCH_TILES}"} \
    ${BENCH_ROWS:+--rows "${BENCH_ROWS}"} \
    ${OUTPUT_FILE:+--output "${OUTPUT_FILE}"}
