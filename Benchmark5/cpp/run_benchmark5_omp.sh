#!/bin/bash
# Benchmark 5 — OpenMP affinity strategies
# Builds openmp_benchmark5.cpp then runs all three proc_bind strategies
# at 8, 16, 32, 64 threads.
#
# Affinity is controlled entirely via the proc_bind clause in the source —
# no OMP_PROC_BIND or OMP_PLACES env vars are set by this script.
#
# Output: ../results_omp_default.csv
#         ../results_omp_spread.csv
#         ../results_omp_close.csv

set -e

SRC="openmp_benchmark5.cpp"
BIN="./openmp_benchmark5"
N=134217728          # 128 M elements = 1 GB
TRIALS=5
THREADS_LIST=(8 16 32 64)
STRATEGIES=(default spread close)

echo "=== Building $SRC ==="
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"
echo ""

for strategy in "${STRATEGIES[@]}"; do
    OUT="../results_omp_${strategy}.csv"
    echo "--- Strategy: $strategy ---"

    # First thread count: write header
    "$BIN" "${THREADS_LIST[0]}" "$N" "$strategy" "$TRIALS" --csv --warmup > "$OUT"

    # Remaining thread counts: append without header
    for t in "${THREADS_LIST[@]:1}"; do
        "$BIN" "$t" "$N" "$strategy" "$TRIALS" --csv --warmup | tail -n +2 >> "$OUT"
    done

    echo "    -> $OUT"
done

echo ""
echo "All OMP runs complete."
