#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"
REPEATS="${2:-5}"

if [[ -z "$MODE" ]]; then
  echo "Usage:"
  echo "  $0 <mode> [repeats] [extra args]"
  echo
  echo "mode:"
  echo "  fat         - collect by --fat-count"
  echo "  bs          - collect by --bs-count"
  echo "  broken-bs   - collect by --b-bs-count"
  echo
  echo "Examples:"
  echo "  $0 fat 10"
  echo "  $0 bs 7 --bs-fixed 32"
  echo "  $0 broken-bs 7 --bs-fixed 32"
  exit 1
fi

RUNNER="test/nifat32_tests.py"
FORMATTER="formatter/"
TESTS_FOLDER="test/"
ROOT_FOLDER="."
OUT_ROOT="collected_results"

COLLECT_FROM=1
COLLECT_TO=16
COLLECT_STEP=1

TEST_NAME=""
COLLECT_PARAM=""
FIXED_ARGS=()
MODE_NAME=""

BS_FIXED=32
BROKEN_BS_FIXED=0

shift 2 || true

while [[ $# -gt 0 ]]; do
  case "$1" in
    --from)
      COLLECT_FROM="$2"
      shift 2
      ;;
    --to)
      COLLECT_TO="$2"
      shift 2
      ;;
    --step)
      COLLECT_STEP="$2"
      shift 2
      ;;
    --runner)
      RUNNER="$2"
      shift 2
      ;;
    --formatter)
      FORMATTER="$2"
      shift 2
      ;;
    --tests-folder)
      TESTS_FOLDER="$2"
      shift 2
      ;;
    --root-folder)
      ROOT_FOLDER="$2"
      shift 2
      ;;
    --out-root)
      OUT_ROOT="$2"
      shift 2
      ;;
    --bs-fixed)
      BS_FIXED="$2"
      shift 2
      ;;
    --broken-bs-fixed)
      BROKEN_BS_FIXED="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1"
      exit 1
      ;;
  esac
done

case "$MODE" in
  fat)
    MODE_NAME="fat"
    TEST_NAME="test_nifat32_cluster_time"
    COLLECT_PARAM="fat-count"
    ;;
  bs)
    MODE_NAME="bs"
    TEST_NAME="test_nifat32_boot"
    COLLECT_PARAM="bs-count"
    FIXED_ARGS+=(--b-bs-count "$BROKEN_BS_FIXED")
    ;;
  broken-bs)
    MODE_NAME="broken-bs"
    TEST_NAME="test_nifat32_boot"
    COLLECT_PARAM="b-bs-count"
    FIXED_ARGS+=(--bs-count "$BS_FIXED")
    ;;
  *)
    echo "Unknown mode: $MODE"
    echo "Allowed values: fat | bs | broken-bs"
    exit 1
    ;;
esac

if [[ ! -f "$RUNNER" ]]; then
  echo "Runner not found: $RUNNER"
  exit 1
fi

TIMESTAMP="$(date '+%Y%m%d_%H%M%S')"
OUT_DIR="${OUT_ROOT}/${MODE_NAME}_${TIMESTAMP}"
mkdir -p "$OUT_DIR"

SUMMARY_FILE="${OUT_DIR}/summary.txt"

{
  echo "mode=${MODE_NAME}"
  echo "repeats=${REPEATS}"
  echo "runner=${RUNNER}"
  echo "test=${TEST_NAME}"
  echo "collect_param=${COLLECT_PARAM}"
  echo "from=${COLLECT_FROM}"
  echo "to=${COLLECT_TO}"
  echo "step=${COLLECT_STEP}"
  echo "fixed_args=${FIXED_ARGS[*]:-<none>}"
  echo
} > "$SUMMARY_FILE"

echo "Results will be saved to: $OUT_DIR"
echo

for ((run=1; run<=REPEATS; run++)); do
  RUN_PADDED="$(printf '%02d' "$run")"
  OUT_FILE="${OUT_DIR}/run_${RUN_PADDED}.txt"
  LOG_FILE="${OUT_DIR}/run_${RUN_PADDED}.log"

  echo "=== Repeat ${run}/${REPEATS} ==="
  echo "Data file: $OUT_FILE"

  CMD=(
    python3 "$RUNNER"
    --new-image
    --formatter "$FORMATTER"
    --tests-folder "$TESTS_FOLDER"
    --root-folder "$ROOT_FOLDER"
    --clean
    --test-specific "$TEST_NAME"
    "${FIXED_ARGS[@]}"
    --collect-param "$COLLECT_PARAM"
    --collect-from "$COLLECT_FROM"
    --collect-to "$COLLECT_TO"
    --collect-step "$COLLECT_STEP"
    --collect-output "$OUT_FILE"
  )

  {
    echo "COMMAND: ${CMD[*]}"
    echo "START: $(date '+%F %T')"
  } > "$LOG_FILE"

  if "${CMD[@]}" >> "$LOG_FILE" 2>&1; then
    echo "STATUS: ok" >> "$LOG_FILE"
    echo "run_${RUN_PADDED}=ok file=${OUT_FILE}" >> "$SUMMARY_FILE"
    echo "OK"
  else
    echo "STATUS: failed" >> "$LOG_FILE"
    echo "run_${RUN_PADDED}=failed file=${OUT_FILE} log=${LOG_FILE}" >> "$SUMMARY_FILE"
    echo "Error in repeat ${run}. See log: $LOG_FILE"
  fi

  echo
done

echo "Done."
echo "Summary: $SUMMARY_FILE"