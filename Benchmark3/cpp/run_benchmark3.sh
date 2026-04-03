#!/bin/bash
# Build and run Benchmark 3 — OpenMP Strategy A (private histogram + merge).
#
# Uses OpenMP array reduction:
#   #pragma omp parallel for reduction(+: h[:BINS])
#
# Results saved to: ../results_openmp.csv
#
# Usage (from Benchmark3/cpp/):
#   chmod +x run_benchmark3.sh
#   ./run_benchmark3.sh

set -e

SRC="openmp_benchmark3.cpp"
BIN="./openmp_benchmark3"

N=67108864   # 2^26 = 67,108,864 elements
BINS=256
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Building..."
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"

OUT="../results_openmp.csv"
echo ""
echo "Running: N=$N, bins=$BINS, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N $BINS $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N $BINS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved → $OUT"
