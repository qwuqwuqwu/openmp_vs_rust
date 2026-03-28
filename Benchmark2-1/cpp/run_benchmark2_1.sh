#!/bin/bash
# Build and run the OpenMP popcount sum benchmark.
# Outputs results to ../results_openmp.csv (Benchmark2-1 root).
#
# Usage (from Benchmark2-1/cpp/):
#   chmod +x run_benchmark2_1.sh
#   ./run_benchmark2_1.sh

set -e

SRC="openmp_benchmark2_1.cpp"
BIN="./openmp_benchmark2_1"
OUT="../results_openmp.csv"

N=8589934592    # 2^33 = 8,589,934,592  (expected answer: 141,733,920,768)
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Compiling..."
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"

echo "Running benchmark (N=$N, trials=$TRIALS)..."

# First thread count: write header + rows
"$BIN" "${THREADS_LIST[0]}" $N $TRIALS --csv --warmup > "$OUT"

# Remaining thread counts: append rows only (skip header)
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved results to $OUT"
