#!/bin/bash

OUT="../results_openmp.csv"
BIN="./openmp_benchmark1"

REGION_REPS=100000
BARRIER_REPS=100000
ATOMIC_REPS=100000
TRIALS=5

THREADS_LIST=(1 2 4 8 16 32)

$BIN "${THREADS_LIST[0]}" $REGION_REPS $BARRIER_REPS $ATOMIC_REPS $TRIALS --csv --warmup > "$OUT"

for t in "${THREADS_LIST[@]:1}"; do
    $BIN "$t" $REGION_REPS $BARRIER_REPS $ATOMIC_REPS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved results to $OUT"
