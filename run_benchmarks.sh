#!/usr/bin/env bash
# =============================================================================
# run_benchmarks.sh  —  Full Knapsack Benchmark Pipeline
# =============================================================================
#
# Steps:
#   1. Compile knapsack_harness.c with -O3
#   2. Optionally generate datasets (if --generate flag is passed)
#   3. Loop over all .txt files in DATASET_DIR
#   4. Run each file RUNS_PER_FILE times, appending CSV rows to RESULTS_FILE
#   5. Automatically skip Brute Force when n > BRUTE_FORCE_N_LIMIT
#
# Usage:
#   ./run_benchmarks.sh                    # benchmark only (datasets must exist)
#   ./run_benchmarks.sh --generate         # generate datasets then benchmark
#   ./run_benchmarks.sh --generate --seed 7 --outdir my_datasets
#
# Options:
#   --generate             Run generate_data.py before benchmarking
#   --seed N               RNG seed passed to generate_data.py (default: 42)
#   --outdir DIR           Dataset directory (default: ./datasets)
#   --runs N               Runs per file for timing averaging (default: 5)
#   --results FILE         Output CSV path (default: ./results.csv)
#   --brute-limit N        Skip BruteForce when n > N (default: 30)
#   --harness FILE         Path to harness source (default: ./knapsack_harness.c)
# =============================================================================

set -euo pipefail

# ─── Defaults ────────────────────────────────────────────────────────────────
GENERATE=0
SEED=42
DATASET_DIR="datasets"
RUNS_PER_FILE=5
RESULTS_FILE="results.csv"
BRUTE_FORCE_N_LIMIT=30
TIMEOUT_SECS=120   # wall-clock seconds before killing a single harness run
HARNESS_SRC="knapsack_harness.c"
HARNESS_BIN="knapsack_harness"
PYTHON="python3"

# ─── Argument parsing ────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --generate)        GENERATE=1 ;;
        --seed)            SEED="$2";        shift ;;
        --outdir)          DATASET_DIR="$2"; shift ;;
        --runs)            RUNS_PER_FILE="$2"; shift ;;
        --results)         RESULTS_FILE="$2"; shift ;;
        --brute-limit)     BRUTE_FORCE_N_LIMIT="$2"; shift ;;
        --timeout)         TIMEOUT_SECS="$2";           shift ;;
        --harness)         HARNESS_SRC="$2"; shift ;;
        -h|--help)
            sed -n '2,40p' "$0"   # print the header comment
            exit 0
            ;;
        *)
            echo "Unknown option: $1  (use --help for usage)" >&2
            exit 1
            ;;
    esac
    shift
done

# ─── Helpers ─────────────────────────────────────────────────────────────────

info()  { echo "[INFO]  $*"; }
warn()  { echo "[WARN]  $*" >&2; }
error() { echo "[ERROR] $*" >&2; exit 1; }

# Extract n and W from the first line of a dataset file.
# Returns two space-separated integers.
read_n_W() {
    head -n 1 "$1"
}

# ─── Step 0: Generate datasets (optional) ────────────────────────────────────
if [[ $GENERATE -eq 1 ]]; then
    info "Generating datasets (seed=$SEED, outdir=$DATASET_DIR) ..."
    "$PYTHON" generate_data.py --seed "$SEED" --outdir "$DATASET_DIR" \
        || error "generate_data.py failed. Aborting."
    info "Dataset generation complete."
fi

# ─── Step 1: Verify datasets exist ───────────────────────────────────────────
if [[ ! -d "$DATASET_DIR" ]]; then
    error "Dataset directory '$DATASET_DIR' not found. Run with --generate first."
fi

mapfile -t ALL_FILES < <(find "$DATASET_DIR" -name "*.txt" | sort)

if [[ ${#ALL_FILES[@]} -eq 0 ]]; then
    error "No .txt files found under '$DATASET_DIR'. Run with --generate first."
fi

info "Found ${#ALL_FILES[@]} dataset file(s) in '$DATASET_DIR'."

# ─── Step 2: Compile ─────────────────────────────────────────────────────────
if [[ ! -f "$HARNESS_SRC" ]]; then
    error "Harness source '$HARNESS_SRC' not found."
fi

info "Compiling $HARNESS_SRC with -O3 ..."
gcc -O3 -Wall -Wextra -o "$HARNESS_BIN" "$HARNESS_SRC" -lm \
    || error "Compilation failed. Aborting."
info "Compiled → ./$HARNESS_BIN"

# ─── Step 3: Initialise results CSV ──────────────────────────────────────────
CSV_HEADER="Algorithm,N,W,MaxValue,Time_Seconds"

if [[ ! -f "$RESULTS_FILE" ]]; then
    echo "$CSV_HEADER" > "$RESULTS_FILE"
    info "Created $RESULTS_FILE with header."
else
    # Verify header matches to avoid silent corruption
    EXISTING_HEADER=$(head -n 1 "$RESULTS_FILE")
    if [[ "$EXISTING_HEADER" != "$CSV_HEADER" ]]; then
        warn "Existing $RESULTS_FILE has unexpected header; appending anyway."
    else
        info "Appending to existing $RESULTS_FILE."
    fi
fi

# ─── Step 4: Benchmark loop ───────────────────────────────────────────────────
TOTAL=${#ALL_FILES[@]}
FILE_IDX=0
SKIPPED_BRUTE=0

for DATASET in "${ALL_FILES[@]}"; do
    FILE_IDX=$(( FILE_IDX + 1 ))
    BASENAME=$(basename "$DATASET")

    # Read n and W from the file header
    read -r FILE_N FILE_W < <(read_n_W "$DATASET")

    # Decide whether to skip Brute Force
    SKIP_FLAG=""
    if [[ "$FILE_N" -gt "$BRUTE_FORCE_N_LIMIT" ]]; then
        SKIP_FLAG="--skip-brute"
        SKIPPED_BRUTE=$(( SKIPPED_BRUTE + 1 ))
        BRUTE_NOTE="[BruteForce SKIPPED: n=$FILE_N > $BRUTE_FORCE_N_LIMIT]"
    else
        BRUTE_NOTE=""
    fi

    info "[$FILE_IDX/$TOTAL] $BASENAME  (n=$FILE_N, W=$FILE_W)  runs=$RUNS_PER_FILE  $BRUTE_NOTE"

    # Run RUNS_PER_FILE times, appending each run's rows directly to the CSV
    for RUN in $(seq 1 "$RUNS_PER_FILE"); do
        # The harness prints 5 CSV rows (one per algorithm) to stdout.
        # We append directly, no temp file needed.
        timeout "$TIMEOUT_SECS" ./"$HARNESS_BIN" "$DATASET" $SKIP_FLAG             >> "$RESULTS_FILE"             || warn "  Run $RUN timed-out or failed for $BASENAME (exit $?) — continuing."
    done

done

# ─── Step 5: Summary ─────────────────────────────────────────────────────────
TOTAL_ROWS=$(( $(wc -l < "$RESULTS_FILE") - 1 ))  # subtract header

echo ""
echo "════════════════════════════════════════════════════════"
echo "  Benchmark complete."
echo "  Datasets run    : $TOTAL"
echo "  Runs per file   : $RUNS_PER_FILE"
echo "  CSV rows written: $TOTAL_ROWS"
echo "  BruteForce skips: $SKIPPED_BRUTE file(s) had n > $BRUTE_FORCE_N_LIMIT"
echo "  Results saved to: $RESULTS_FILE"
echo "════════════════════════════════════════════════════════"
