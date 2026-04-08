#!/bin/bash
# Build and run Benchmark 4 — Rust Prime Testing.
#
# Three scheduling strategies in one binary:
#   static  — std::thread + manual fixed-range partitioning
#             (equivalent to OpenMP schedule(static))
#   dynamic — std::thread + Arc<AtomicU64> shared counter
#             (equivalent to OpenMP schedule(dynamic, chunk_size))
#   rayon   — Rayon par_iter with work-stealing
#             (no direct OpenMP equivalent — automatic load balancing)
#
# Results saved to:
#   ../results_rust_static.csv
#   ../results_rust_dynamic.csv
#   ../results_rust_rayon.csv
#
# Usage (from Benchmark4/rust/):
#   chmod +x run_benchmark4_rust.sh
#   ./run_benchmark4_rust.sh

set -e

BIN="./target/release/rust_benchmark4"

N=1000000       # test primes in [2, 1,000,000] — expected: 78,498 primes
CHUNK=100       # chunk size for dynamic scheduling
TRIALS=5
THREADS_LIST=(1 2 4 8 16 32 64)

echo "Building..."
cargo build --release 2>&1
echo "Done. Binary: $BIN"

# ── Static scheduling ────────────────────────────────────────────────────────
OUT="../results_rust_static.csv"
echo ""
echo "Running: schedule=static, N=$N, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N static 0 $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N static 0 $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done
echo "Saved → $OUT"

# ── Dynamic scheduling ───────────────────────────────────────────────────────
OUT="../results_rust_dynamic.csv"
echo ""
echo "Running: schedule=dynamic (chunk=$CHUNK), N=$N, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N dynamic $CHUNK $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N dynamic $CHUNK $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done
echo "Saved → $OUT"

# ── Rayon (work-stealing) ────────────────────────────────────────────────────
OUT="../results_rust_rayon.csv"
echo ""
echo "Running: schedule=rayon, N=$N, trials=$TRIALS..."

"$BIN" "${THREADS_LIST[0]}" $N rayon 0 $TRIALS --csv --warmup > "$OUT"
for t in "${THREADS_LIST[@]:1}"; do
    "$BIN" "$t" $N rayon 0 $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done
echo "Saved → $OUT"

echo ""
echo "All done."
