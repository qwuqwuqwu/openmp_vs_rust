#!/bin/bash
# Build and run Benchmark 4 — OpenMP Prime Testing.
#
# Runs two scheduling strategies back-to-back:
#   static      — fixed chunks, reveals load imbalance
#   dynamic 100 — runtime work queue, corrects load imbalance
#
# Results saved to:
#   ../results_openmp_static.csv
#   ../results_openmp_dynamic.csv
#
# Usage (from Benchmark4/cpp/):
#   chmod +x run_benchmark4.sh
#   ./run_benchmark4.sh

set -e

SRC="openmp_benchmark4.cpp"
BIN="./openmp_benchmark4"

N=1000000       # test primes in [2, 1,000,000]  — expected: 78,498 primes
CHUNK=100       # chunk size for dynamic scheduling
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Building..."
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"

# ── Static scheduling ────────────────────────────────────────────────────────
OUT="../results_openmp_static.csv"
echo ""
echo "Running: schedule=static, N=$N, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N static 0 $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N static 0 $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done
echo "Saved → $OUT"

# ── Dynamic scheduling ───────────────────────────────────────────────────────
OUT="../results_openmp_dynamic.csv"
echo ""
echo "Running: schedule=dynamic (chunk=$CHUNK), N=$N, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N dynamic $CHUNK $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N dynamic $CHUNK $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done
echo "Saved → $OUT"

echo ""
echo "All done."
