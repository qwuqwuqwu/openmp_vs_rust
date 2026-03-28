#!/bin/bash
# Build and run the OpenMP numerical integration benchmark.
# Outputs results to ../results_openmp.csv (Benchmark2-1 root).
#
# Usage (from Benchmark2-1/cpp/):
#   chmod +x run_benchmark2_1.sh
#   ./run_benchmark2_1.sh

set -e

SRC="openmp_benchmark2_1.cpp"
BIN="./openmp_benchmark2_1"
OUT="../results_openmp.csv"

INTERVALS=1000000000   # 1 billion intervals
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Compiling..."
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"

echo "Running benchmark..."

# First thread count: write header + rows
"$BIN" "${THREADS_LIST[0]}" $INTERVALS $TRIALS --csv --warmup > "$OUT"

# Remaining thread counts: append rows only (skip header)
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $INTERVALS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved results to $OUT"
