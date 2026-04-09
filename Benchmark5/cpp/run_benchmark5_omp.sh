#!/bin/bash
# Benchmark 5 — OpenMP affinity strategies
# Builds openmp_benchmark5.cpp then runs all three proc_bind strategies
# at 8, 16, 32, 64 threads.
#
# Affinity is controlled entirely via the proc_bind clause in the source —
# no OMP_PROC_BIND or OMP_PLACES env vars are set by this script.
#
# Output: ../results_omp_default.csv
#         ../results_omp_spread.csv
#         ../results_omp_close.csv

set -e

SRC="openmp_benchmark5.cpp"
BIN="./openmp_benchmark5"
N=134217728          # 128 M elements = 1 GB
TRIALS=5
THREADS_LIST=(8 16 32 64)
STRATEGIES=(default spread close)

echo "=== Building $SRC ==="
g++ -O3 -fopenmp -std=c++17 -o "$BIN" "$SRC"
echo "Done. Binary: $BIN"
echo ""

# ── Inner-loop disassembly ────────────────────────────────────────────────
# GCC with OpenMP "outlines" each parallel region into a separate function.
# The naming pattern on libgomp is typically:
#   sum_spread._omp_fn.N  or  __omp_outlined__N
# We also dump the named wrappers (sum_spread etc.) for context.
# All three sum_* bodies are identical; we inspect sum_spread as the canonical one.
DISASM_FILE="../disasm_omp.txt"
echo "=== Disassembly: GCC inner loop (saved to $DISASM_FILE) ==="
objdump -d -M intel -C "$BIN" > "$DISASM_FILE"

echo "-- SIMD register summary --"
printf "  ymm (AVX  256-bit) instructions : %d\n" "$(grep -c 'ymm' "$DISASM_FILE" || true)"
printf "  xmm (SSE  128-bit) instructions : %d\n" "$(grep -c 'xmm' "$DISASM_FILE" || true)"
printf "  zmm (AVX-512 512-bit) instructions: %d\n" "$(grep -c 'zmm' "$DISASM_FILE" || true)"
echo ""

echo "-- sum_spread function (outlined parallel region + wrapper) --"
# Extract the outlined body GCC generates for the parallel region inside sum_spread.
# GCC names it with a suffix like ._omp_fn.N; we grab whichever follows sum_spread.
awk '
  /^[[:xdigit:]]+ <.*sum_spread.*>:/ { found=1; count=0 }
  found { print; count++ }
  found && count > 80 { print "  ... (truncated)"; exit }
  /^[[:xdigit:]]+ <[^>]+>:/ && found && !/sum_spread/ { exit }
' "$DISASM_FILE"
echo ""

for strategy in "${STRATEGIES[@]}"; do
    OUT="../results_omp_${strategy}.csv"
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
echo "All OMP runs complete."
