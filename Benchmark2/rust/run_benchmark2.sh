#!/bin/bash

OUT="../results_rust.csv"
BIN="./target/release/rust_benchmark2"

SAMPLES=100000000
TRIALS=5

THREADS_LIST=(1 2 4 8 16 32 64)

$BIN "${THREADS_LIST[0]}" $SAMPLES $TRIALS --csv --warmup > "$OUT"

for t in "${THREADS_LIST[@]:1}"; do
    $BIN "$t" $SAMPLES $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved results to $OUT"
