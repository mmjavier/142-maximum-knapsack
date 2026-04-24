#!/usr/bin/env bash
# =============================================================================
# run_benchmark.sh — Knapsack Algorithm Benchmark Runner
# =============================================================================
# Compiles all 5 knapsack C implementations, generates datasets (if needed),
# then runs each algorithm on every dataset file 5 times, recording the
# elapsed time (ms) from each run's TIME_MS output line.
#
# Output: results.csv
#   Columns: Algorithm,N,W,MaxValue,Run1,Run2,Run3,Run4,Run5,Ave
#
# Safeguards:
#   - Brute Force is skipped automatically when n > 30
#   - 2D DP is skipped when the table would exceed MEM_LIMIT_MB (default 512)
#   - Each run has a TIMEOUT_SEC (default 60 s) hard limit
# =============================================================================

set -euo pipefail

# ── Tunables ──────────────────────────────────────────────────────────────────
DATASET_DIR="${DATASET_DIR:-./datasets}"
RESULTS_CSV="${RESULTS_CSV:-results.csv}"
RUNS=5
TIMEOUT_SEC=60          # wall-clock seconds before a run is killed
MEM_LIMIT_MB=512        # skip 2D DP if table > this many MB
BF_N_LIMIT=30           # skip Brute Force when n exceeds this
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

# ── Dependency checks ─────────────────────────────────────────────────────────
for cmd in gcc python3 bc; do
    command -v "$cmd" &>/dev/null || { err "Required command not found: $cmd"; exit 1; }
done

# ── Step 1 — Compile ──────────────────────────────────────────────────────────
log "Compiling C sources with gcc -O3 ..."

declare -A BIN_MAP=(
    [BruteForce]="01_brute_force.c"
    [DP_2D]="02_dp_2d.c"
    [DP_1D]="03_dp_1d.c"
    [Greedy]="04_greedy.c"
    [BranchBound]="05_branch_bound.c"
)

declare -A BIN_PATH

for algo in "${!BIN_MAP[@]}"; do
    src="${BIN_MAP[$algo]}"
    bin="./bin_${algo}"
    if gcc -O3 -o "$bin" "$src" -lm 2>/dev/null; then
        ok "  $algo  ← $src"
        BIN_PATH[$algo]="$bin"
    else
        err "  Failed to compile $src — $algo will be skipped."
    fi
done

# ── Step 2 — Generate datasets (if directory is absent or empty) ──────────────
if [[ ! -d "$DATASET_DIR" ]] || [[ -z "$(find "$DATASET_DIR" -name '*.txt' 2>/dev/null)" ]]; then
    log "Dataset directory '$DATASET_DIR' is empty or missing — running generate_data.py ..."
    python3 generate_data.py --outdir "$DATASET_DIR"
else
    log "Using existing datasets in '$DATASET_DIR'."
fi

# ── Step 3 — Collect all .txt dataset files ───────────────────────────────────
mapfile -t ALL_FILES < <(find "$DATASET_DIR" -name '*.txt' | sort)

if [[ ${#ALL_FILES[@]} -eq 0 ]]; then
    err "No .txt files found under '$DATASET_DIR'. Aborting."
    exit 1
fi
log "Found ${#ALL_FILES[@]} dataset file(s)."

# ── Step 4 — Initialise CSV ───────────────────────────────────────────────────
if [[ ! -f "$RESULTS_CSV" ]]; then
    echo "Algorithm,N,W,MaxValue,Run1,Run2,Run3,Run4,Run5,Ave" > "$RESULTS_CSV"
    log "Created $RESULTS_CSV with header."
else
    log "Appending to existing $RESULTS_CSV."
fi

# ── Helper: extract a value from program output ───────────────────────────────
extract_max_value() {
    # Looks for lines like:  Max Value   : 1234  /  Max Value    : 1234
    grep -oP '(?<=Max Value\s{1,6}:\s)\d+' <<< "$1" | head -1
}

extract_time_ms() {
    grep -oP '(?<=TIME_MS:)[\d.]+' <<< "$1" | head -1
}

# ── Helper: read n and W from a dataset file ──────────────────────────────────
read_n_w() {
    local file="$1"
    local W n
    # Format: Line 1 = W,  Line 2 = n  (space-separated on same or separate lines)
    read -r W  < <(sed -n '1p' "$file")
    read -r n  < <(sed -n '2p' "$file")
    echo "$n $W"
}

# ── Step 5 — Main benchmark loop ──────────────────────────────────────────────
total_files=${#ALL_FILES[@]}
file_idx=0

for dataset in "${ALL_FILES[@]}"; do
    (( file_idx++ )) || true
    fname=$(basename "$dataset")
    read -r n W <<< "$(read_n_w "$dataset")"

    echo ""
    log "${CYN}[$file_idx/$total_files]${RST} $fname  (n=$n, W=$W)"

    # ── Determine algo order (BruteForce last so slowdowns don't block others) ─
    algo_order=(Greedy DP_1D DP_2D BranchBound BruteForce)

    for algo in "${algo_order[@]}"; do
        bin="${BIN_PATH[$algo]:-}"

        # Skip if binary didn't compile
        if [[ -z "$bin" ]]; then
            warn "    [$algo] binary missing — skip."
            continue
        fi

        # ── Safeguard: Brute Force n > BF_N_LIMIT ─────────────────────────────
        if [[ "$algo" == "BruteForce" ]] && (( n > BF_N_LIMIT )); then
            warn "    [$algo] n=$n > $BF_N_LIMIT — SKIPPED (would run ~2^$n iterations)."
            echo "$algo,$n,$W,SKIPPED(n>$BF_N_LIMIT),,,,,," >> "$RESULTS_CSV"
            continue
        fi

        # ── Safeguard: 2D DP memory wall ──────────────────────────────────────
        if [[ "$algo" == "DP_2D" ]]; then
            table_mb=$(( (n + 1) * (W + 1) * 4 / 1024 / 1024 ))
            if (( table_mb > MEM_LIMIT_MB )); then
                warn "    [$algo] table ~${table_mb}MB > limit ${MEM_LIMIT_MB}MB — SKIPPED."
                echo "$algo,$n,$W,SKIPPED(mem>${MEM_LIMIT_MB}MB),,,,,," >> "$RESULTS_CSV"
                continue
            fi
        fi

        echo -n "    [$algo]"

        times=()
        max_value="N/A"
        run_failed=0

        for (( run=1; run<=RUNS; run++ )); do
            output=$(timeout "$TIMEOUT_SEC" "$bin" "$dataset" 2>/dev/null) || {
                rc=$?
                if [[ $rc -eq 124 ]]; then
                    warn " run$run=TIMEOUT(>${TIMEOUT_SEC}s)"
                else
                    warn " run$run=ERROR(rc=$rc)"
                fi
                times+=("FAIL")
                run_failed=1
                continue
            }

            t=$(extract_time_ms "$output")
            if [[ -z "$t" ]]; then
                warn " run$run=NO_TIME"
                times+=("FAIL")
                run_failed=1
                continue
            fi
            times+=("$t")
            echo -n " ${t}ms"

            # Capture MaxValue from first successful run
            if [[ "$max_value" == "N/A" ]]; then
                mv=$(extract_max_value "$output")
                [[ -n "$mv" ]] && max_value="$mv"
            fi
        done

        # Compute average (skip FAIL entries)
        valid_times=()
        for t in "${times[@]}"; do
            [[ "$t" != "FAIL" ]] && valid_times+=("$t")
        done

        if (( ${#valid_times[@]} > 0 )); then
            sum_expr=$(IFS=+; echo "${valid_times[*]}")
            ave=$(echo "scale=4; ($sum_expr) / ${#valid_times[@]}" | bc)
        else
            ave="N/A"
        fi

        echo "  → Ave=${ave}ms  MaxValue=${max_value}"

        # Pad times array to exactly RUNS entries
        while (( ${#times[@]} < RUNS )); do
            times+=("N/A")
        done

        # Write CSV row
        printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
            "$algo" "$n" "$W" "$max_value" \
            "${times[0]}" "${times[1]}" "${times[2]}" "${times[3]}" "${times[4]}" \
            "$ave" >> "$RESULTS_CSV"
    done
done

echo ""
ok "Benchmark complete. Results saved to: ${BOLD}$RESULTS_CSV${RST}"
echo ""
echo "Quick summary (first 20 rows):"
echo "────────────────────────────────────────────────────────────────"
column -t -s',' "$RESULTS_CSV" 2>/dev/null | head -21 || head -21 "$RESULTS_CSV"
echo "────────────────────────────────────────────────────────────────"