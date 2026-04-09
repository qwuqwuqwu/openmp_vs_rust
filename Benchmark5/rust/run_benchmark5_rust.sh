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

# ── Inner-loop disassembly ────────────────────────────────────────────────
# LLVM compiles the iter().sum::<u64>() call in the parallel_sum closure.
# We look for two things:
#   1. The closure symbol (mangled; -C flag demangles it)
#   2. The std::iter::Sum for u64 implementation, which is what actually loops
#
# Key things to look for vs GCC output:
#   - ymm/xmm registers → SIMD vectorization
#   - Multiple add/paddq per loop iteration → unrolling
#   - prefetch instructions → LLVM software prefetch
DISASM_FILE="../disasm_rust.txt"
echo "=== Disassembly: LLVM inner loop (saved to $DISASM_FILE) ==="
objdump -d -M intel -C "$BIN" > "$DISASM_FILE"

echo "-- SIMD register summary --"
printf "  ymm (AVX  256-bit) instructions : %d\n" "$(grep -c 'ymm' "$DISASM_FILE" || true)"
printf "  xmm (SSE  128-bit) instructions : %d\n" "$(grep -c 'xmm' "$DISASM_FILE" || true)"
printf "  zmm (AVX-512 512-bit) instructions: %d\n" "$(grep -c 'zmm' "$DISASM_FILE" || true)"
echo ""

echo "-- parallel_sum closure (the thread body that runs iter().sum) --"
# The closure is emitted as rust_benchmark5::parallel_sum::{{closure}} (demangled).
# We grab the first matching function and show up to 100 lines.
awk '
  /parallel_sum.*closure/ { found=1; count=0 }
  found { print; count++ }
  found && count > 100 { print "  ... (truncated)"; exit }
  /^[[:xdigit:]]+ <[^>]+>:/ && found && !/parallel_sum/ { exit }
' "$DISASM_FILE"
echo ""

echo "-- All symbols containing 'sum' (to help locate the reduction loop) --"
grep '^[[:xdigit:]]* <.*sum' "$DISASM_FILE" | head -20
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
