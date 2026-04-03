#!/bin/bash
# Build and run Benchmark 3 — Rust Strategy A (private histogram + merge).
#
# Results saved to: ../results_rust.csv
#
# Usage (from Benchmark3/rust/):
#   chmod +x run_benchmark3.sh
#   ./run_benchmark3.sh

set -e

BIN="./target/release/rust_benchmark3"

N=67108864   # 2^26 = 67,108,864 elements  (same as OpenMP run)
BINS=256
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Building..."
cargo build --release 2>&1
echo "Done. Binary: $BIN"

OUT="../results_rust.csv"
echo ""
echo "Running: N=$N, bins=$BINS, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N $BINS $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N $BINS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved → $OUT"
