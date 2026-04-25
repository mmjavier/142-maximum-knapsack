#!/usr/bin/env bash
# =============================================================================
# run_benchmarks.sh — Knapsack Algorithm Benchmark Runner (macOS-compatible)
# =============================================================================
# Compiles all 5 knapsack C implementations, generates datasets (if needed),
# then runs each algorithm on every dataset file, recording elapsed time
# (seconds) and analytical peak memory (MB) from each run's output.
#
# Output: results.csv
#   Columns: Algorithm,N,W,MaxValue,Time_Seconds,Memory_MB
#
# Safeguards:
#   - Brute Force is skipped automatically when n > BF_N_LIMIT (default 30)
#   - 2D DP is skipped when the table would exceed MEM_LIMIT_MB (default 512)
#   - Each run has a TIMEOUT_SEC (default 120 s) hard limit
#   - macOS-safe timeout via background process + kill
# =============================================================================

set -uo pipefail

# ── Tunables ──────────────────────────────────────────────────────────────────
DATASET_DIR="${DATASET_DIR:-./datasets}"
RESULTS_CSV="${RESULTS_CSV:-results.csv}"
RUNS=5
TIMEOUT_SEC=10800       # wall-clock seconds before a run is killed (3 hours)
MEM_LIMIT_MB=512        # skip 2D DP if table > this many MB
BF_N_LIMIT=35           # skip Brute Force when n exceeds this
# ─────────────────────────────────────────────────────────────────────────────

# ── Colours (disabled if not a terminal) ─────────────────────────────────────
if [[ -t 1 ]]; then
    RED='\033[0;31m'; YEL='\033[0;33m'; GRN='\033[0;32m'
    CYN='\033[0;36m'; BOLD='\033[1m'; RST='\033[0m'
else
    RED=''; YEL=''; GRN=''; CYN=''; BOLD=''; RST=''
fi

log()  { echo -e "${BOLD}[BENCH]${RST} $*"; }
warn() { echo -e "${YEL}[WARN]${RST}  $*"; }
err()  { echo -e "${RED}[ERR]${RST}   $*" >&2; }
ok()   { echo -e "${GRN}[OK]${RST}    $*"; }

# ── macOS-safe timeout helper ─────────────────────────────────────────────────
# Runs "$@" with a wall-clock timeout. Exits with the command's exit code, or
# 124 if the timeout fires. Works on macOS without coreutils.
run_with_timeout() {
    local secs="$1"; shift
    # If gtimeout (coreutils) is available, prefer it
    if command -v gtimeout &>/dev/null; then
        gtimeout "$secs" "$@"
        return $?
    fi
    # If GNU timeout is available (Linux or homebrew timeout)
    if command -v timeout &>/dev/null; then
        timeout "$secs" "$@"
        return $?
    fi
    # Pure-bash fallback: run in background, wait, kill if needed
    "$@" &
    local pid=$!
    (
        sleep "$secs"
        kill -TERM "$pid" 2>/dev/null
        sleep 1
        kill -KILL "$pid" 2>/dev/null
    ) &
    local watcher_pid=$!
    wait "$pid" 2>/dev/null
    local rc=$?
    # Kill the watcher if the process finished before timeout
    kill "$watcher_pid" 2>/dev/null
    wait "$watcher_pid" 2>/dev/null
    if [[ $rc -eq 0 || $rc -lt 128 ]]; then
        return $rc
    fi
    # SIGTERM gives rc=143, SIGKILL gives rc=137 on macOS
    return 124
}

# ── Dependency checks ─────────────────────────────────────────────────────────
for cmd in gcc python3; do
    command -v "$cmd" &>/dev/null || { err "Required command not found: $cmd"; exit 1; }
done

# bc is optional — we use awk for arithmetic if bc is absent
USE_BC=0
command -v bc &>/dev/null && USE_BC=1

# ── Step 1 — Compile ──────────────────────────────────────────────────────────
log "Compiling C sources with gcc -O3 ..."

rm -f ./bin_BruteForce ./bin_DP_2D ./bin_DP_1D ./bin_Greedy ./bin_BranchBound

ALGOS=(
    "BruteForce:01_brute_force.c"
    "DP_2D:02_dp_2d.c"
    "DP_1D:03_dp_1d.c"
    "Greedy:04_greedy.c"
    "BranchBound:05_branch_bound.c"
)

compile_ok=0
for pair in "${ALGOS[@]}"; do
    algo="${pair%%:*}"
    src="${pair##*:}"
    bin="./bin_${algo}"
    if [[ ! -f "$src" ]]; then
        err "  Source file '$src' not found — $algo will be skipped."
        continue
    fi
    if gcc -O3 -o "$bin" "$src" -lm 2>/tmp/gcc_err_${algo}.txt; then
        ok "  $algo  ← $src"
        (( compile_ok++ )) || true
    else
        err "  Failed to compile $src — $algo will be skipped."
        cat /tmp/gcc_err_${algo}.txt >&2
    fi
done

if (( compile_ok == 0 )); then
    err "No algorithms compiled successfully. Aborting."
    exit 1
fi

# ── Step 2 — Generate datasets (if directory is absent or empty) ──────────────
if [[ ! -d "$DATASET_DIR" ]] || [[ -z "$(find "$DATASET_DIR" -name '*.txt' 2>/dev/null | head -1)" ]]; then
    log "Dataset directory '$DATASET_DIR' is empty or missing — running generate_data.py ..."
    python3 generate_data.py --outdir "$DATASET_DIR"
else
    log "Using existing datasets in '$DATASET_DIR'."
fi

# ── Step 3 — Collect all .txt dataset files ───────────────────────────────────
ALL_FILES=()
while IFS= read -r line; do
    [[ -n "$line" ]] && ALL_FILES+=("$line")
done < <(find "$DATASET_DIR" -name '*.txt' | sort)

if [[ ${#ALL_FILES[@]} -eq 0 ]]; then
    err "No .txt files found under '$DATASET_DIR'. Aborting."
    exit 1
fi
log "Found ${#ALL_FILES[@]} dataset file(s)."

# ── Step 4 — Initialise CSV ───────────────────────────────────────────────────
echo "Algorithm,N,W,MaxValue,Time_Seconds,Memory_MB" > "$RESULTS_CSV"
log "Created $RESULTS_CSV with header."

# ── Helper: extract a value from program output ───────────────────────────────
extract_max_value() {
    # Greedy prints "Max Value    : N"; others print "Max Value  : N" or "Max Value   : N"
    echo "$1" | awk -F':' '/Max Value/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2; exit }'
}

extract_time_sec() {
    echo "$1" | awk -F'TIME_SEC:' '/TIME_SEC:/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2; exit }'
}

extract_memory_mb() {
    echo "$1" | awk -F'MEMORY_MB:' '/MEMORY_MB:/ { gsub(/^[ \t]+|[ \t]+$/, "", $2); print $2; exit }'
}

# ── Helper: compute average with awk (no bc dependency) ──────────────────────
awk_average() {
    # $@ = list of numeric values
    local vals=("$@")
    local n=${#vals[@]}
    if (( n == 0 )); then echo "N/A"; return; fi
    local sum_expr
    printf -v sum_expr '%s+' "${vals[@]}"
    sum_expr="${sum_expr%+}"  # strip trailing '+'
    awk "BEGIN { printf \"%.6f\", ($sum_expr) / $n }"
}

# ── Helper: read n and W from a dataset file ──────────────────────────────────
read_n_w() {
    local file="$1"
    local W n
    read -r W < <(sed -n '1p' "$file")
    read -r n < <(sed -n '2p' "$file")
    echo "$n $W"
}

# ── Step 5 — Main benchmark loop ──────────────────────────────────────────────
total_files=${#ALL_FILES[@]}
file_idx=0

for dataset in "${ALL_FILES[@]}"; do
    (( file_idx++ )) || true
    fname=$(basename "$dataset")
    read -r n W <<< "$(read_n_w "$dataset")"

    # Validate parsed values
    if ! [[ "$n" =~ ^[0-9]+$ ]] || ! [[ "$W" =~ ^[0-9]+$ ]]; then
        warn "Could not parse n/W from '$fname' — skipping."
        continue
    fi

    echo ""
    log "${CYN}[$file_idx/$total_files]${RST} $fname  (n=$n, W=$W)"

    # Run algorithms: fast ones first, BruteForce last
    algo_order=(Greedy DP_1D DP_2D BranchBound BruteForce)

    for algo in "${algo_order[@]}"; do
        bin="./bin_${algo}"

        # Skip if binary didn't compile
        if [[ ! -x "$bin" ]]; then
            warn "    [$algo] binary missing — skip."
            continue
        fi

        # ── Safeguard: Brute Force n > BF_N_LIMIT ─────────────────────────────
        if [[ "$algo" == "BruteForce" ]] && (( n > BF_N_LIMIT )); then
            warn "    [$algo] n=$n > $BF_N_LIMIT — SKIPPED (would run ~2^$n iterations)."
            printf "%s,%s,%s,%s,%s,%s\n" \
                "$algo" "$n" "$W" "SKIPPED(n>$BF_N_LIMIT)" "N/A" "N/A" >> "$RESULTS_CSV"
            continue
        fi

        # ── Safeguard: 2D DP memory wall ──────────────────────────────────────
        if [[ "$algo" == "DP_2D" ]]; then
            # Use awk to avoid integer overflow on large n*W products
            table_mb=$(awk "BEGIN { printf \"%.0f\", (($n + 1.0) * ($W + 1.0) * 4) / 1048576 }")
            if (( table_mb > MEM_LIMIT_MB )); then
                warn "    [$algo] table ~${table_mb}MB > limit ${MEM_LIMIT_MB}MB — SKIPPED."
                printf "%s,%s,%s,%s,%s,%s\n" \
                    "$algo" "$n" "$W" "SKIPPED(mem>${MEM_LIMIT_MB}MB)" "N/A" "N/A" >> "$RESULTS_CSV"
                continue
            fi
        fi

        echo -n "    [$algo]"

        all_times=()
        all_mems=()
        max_value="N/A"
        any_success=0

        for (( run=1; run<=RUNS; run++ )); do
            output=""
            run_with_timeout "$TIMEOUT_SEC" "$bin" "$dataset" > /tmp/bench_out_$$.txt 2>/dev/null
            rc=$?
            output=$(cat /tmp/bench_out_$$.txt 2>/dev/null || true)
            rm -f /tmp/bench_out_$$.txt

            if [[ $rc -eq 124 ]]; then
                warn " run$run=TIMEOUT(>${TIMEOUT_SEC}s)"
                all_times+=("TIMEOUT")
                all_mems+=("N/A")
                continue
            elif [[ $rc -ne 0 ]]; then
                warn " run$run=ERROR(rc=$rc)"
                all_times+=("FAIL")
                all_mems+=("N/A")
                continue
            fi

            t=$(extract_time_sec "$output")
            m=$(extract_memory_mb "$output")

            if [[ -z "$t" ]]; then
                warn " run$run=NO_TIME_SEC"
                all_times+=("FAIL")
                all_mems+=("N/A")
                continue
            fi

            all_times+=("$t")
            any_success=1
            echo -n " ${t}s"

            # Capture MaxValue and Memory from first successful run
            if [[ "$max_value" == "N/A" ]]; then
                mv=$(extract_max_value "$output")
                [[ -n "$mv" ]] && max_value="$mv"
            fi
            if [[ -z "$m" ]]; then m="N/A"; fi
            all_mems+=("$m")
        done

        # Compute average time across successful runs
        valid_times=()
        for t in "${all_times[@]}"; do
            [[ "$t" != "FAIL" && "$t" != "TIMEOUT" && "$t" != "N/A" ]] && valid_times+=("$t")
        done

        if (( ${#valid_times[@]} > 0 )); then
            ave_time=$(awk_average "${valid_times[@]}")
        else
            ave_time="N/A"
        fi

        # Use memory from first successful run (it's deterministic / analytical)
        mem_out="N/A"
        for m in "${all_mems[@]}"; do
            if [[ "$m" != "N/A" && -n "$m" ]]; then
                mem_out="$m"
                break
            fi
        done

        echo "  → Ave=${ave_time}s  MaxValue=${max_value}  Mem=${mem_out}MB"

        # Write CSV row: Algorithm,N,W,MaxValue,Time_Seconds,Memory_MB
        printf "%s,%s,%s,%s,%s,%s\n" \
            "$algo" "$n" "$W" "$max_value" "$ave_time" "$mem_out" >> "$RESULTS_CSV"
    done
done

rm -f /tmp/bench_out_$$.txt 2>/dev/null

echo ""
ok "Benchmark complete. Results saved to: ${BOLD}$RESULTS_CSV${RST}"
echo ""
echo "Quick summary (first 20 rows):"
echo "────────────────────────────────────────────────────────────────────────"
column -t -s',' "$RESULTS_CSV" 2>/dev/null | head -21 || head -21 "$RESULTS_CSV"
echo "────────────────────────────────────────────────────────────────────────"