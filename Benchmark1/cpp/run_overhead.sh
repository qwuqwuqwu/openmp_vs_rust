#!/bin/bash

set -e

SRC="openmp_benchmark1.cpp"
BIN="./openmp_benchmark1"
OUT="../results_openmp.csv"

REGION_REPS=100000
BARRIER_REPS=100000
ATOMIC_REPS=100000
TRIALS=5

THREADS_LIST=(1 2 4 8 16 32)

# Build
echo "Building $SRC..."
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"
echo ""

# Run
echo "Running: region_reps=$REGION_REPS, barrier_reps=$BARRIER_REPS, atomic_reps=$ATOMIC_REPS, trials=$TRIALS..."

$BIN "${THREADS_LIST[0]}" $REGION_REPS $BARRIER_REPS $ATOMIC_REPS $TRIALS --csv --warmup > "$OUT"

for t in "${THREADS_LIST[@]:1}"; do
    $BIN "$t" $REGION_REPS $BARRIER_REPS $ATOMIC_REPS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved results to $OUT"
