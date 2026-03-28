#!/bin/bash
# Build and run the Rayon popcount benchmark (Benchmark 2-2).
# Outputs results to ../results_rust.csv (Benchmark2-2 root).
#
# Usage (from Benchmark2-2/rust/):
#   chmod +x run_benchmark2_2.sh
#   ./run_benchmark2_2.sh

set -e

BIN="./target/release/rust_benchmark2_2"
OUT="../results_rust.csv"

N=8589934592    # 2^33 = 8,589,934,592  (expected answer: 141,733,920,768)
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Building..."
cargo build --release 2>&1
echo "Done. Binary: $BIN"

echo "Running benchmark (N=$N, trials=$TRIALS)..."

# First thread count: write header + rows
"$BIN" "${THREADS_LIST[0]}" $N $TRIALS --csv --warmup > "$OUT"

# Remaining thread counts: append rows only (skip header)
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved results to $OUT"
