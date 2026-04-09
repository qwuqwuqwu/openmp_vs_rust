#!/bin/bash
# Benchmark 5 — Rust affinity strategies
# Builds rust_benchmark5 then runs all three strategies
# (default / spread / close) at 8, 16, 32, 64 threads.
#
# Affinity is controlled via the core_affinity crate inside the binary.
# No external env vars needed (unlike OMP_PROC_BIND approach).
#
# Output: ../results_rust_default.csv
#         ../results_rust_spread.csv
#         ../results_rust_close.csv

set -e

BIN="./target/release/rust_benchmark5"
N=134217728          # 128 M elements = 1 GB
TRIALS=5
THREADS_LIST=(8 16 32 64)
STRATEGIES=(default spread close)

echo "=== Building rust_benchmark5 ==="
cargo build --release 2>&1
echo "Done. Binary: $BIN"
echo ""

for strategy in "${STRATEGIES[@]}"; do
    OUT="../results_rust_${strategy}.csv"
    echo "--- Strategy: $strategy ---"

    # First thread count: write header
    "$BIN" "${THREADS_LIST[0]}" "$N" "$strategy" "$TRIALS" --csv --warmup > "$OUT"

    # Remaining thread counts: append without header
    for t in "${THREADS_LIST[@]:1}"; do
        "$BIN" "$t" "$N" "$strategy" "$TRIALS" --csv --warmup | tail -n +2 >> "$OUT"
    done

    echo "    -> $OUT"
done

echo ""
echo "All Rust runs complete."
