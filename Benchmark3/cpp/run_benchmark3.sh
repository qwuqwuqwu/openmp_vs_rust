#!/bin/bash
# Build and run Benchmark 3 — OpenMP Atomic Histogram (Strategy B).
#
# Runs two separate bin configurations to compare:
#   16   bins → high contention  (many threads collide on few buckets)
#   1024 bins → low  contention  (threads rarely collide)
#
# Results saved to:
#   ../results_openmp_bins16.csv
#   ../results_openmp_bins1024.csv
#
# Usage (from Benchmark3/cpp/):
#   chmod +x run_benchmark3.sh
#   ./run_benchmark3.sh

set -e

SRC="openmp_benchmark3.cpp"
BIN="./openmp_benchmark3"

N=67108864      # 2^26 = 67,108,864 elements
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Building..."
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"

# ── High contention: 16 bins ────────────────────────────────────────────────
BINS=16
OUT="../results_openmp_bins16.csv"
echo ""
echo "Running: $BINS bins (high contention), N=$N, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N $BINS $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N $BINS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done
echo "Saved → $OUT"

# ── Low contention: 1024 bins ───────────────────────────────────────────────
BINS=1024
OUT="../results_openmp_bins1024.csv"
echo ""
echo "Running: $BINS bins (low contention), N=$N, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N $BINS $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N $BINS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done
echo "Saved → $OUT"

echo ""
echo "All done."
