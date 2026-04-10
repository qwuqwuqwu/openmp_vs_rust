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

# ── Disassembly analysis ─────────────────────────────────────────────────────
DISASM_FILE="../disasm_omp.txt"
echo "=== Disassembly: $BIN → $DISASM_FILE ==="
objdump -d -C "$BIN" > "$DISASM_FILE"
echo "Full disasm saved."

echo ""
echo "── OMP barrier call site (benchmark_barrier) ──────────────────────────"
# Extract the benchmark_barrier function body and look for the GOMP_barrier call.
# 'GOMP_barrier' is the libgomp entry point compiled from '#pragma omp barrier'.
# It should appear as a 'callq' / 'call' instruction inside the barrier loop.
objdump -d -C "$BIN" \
  | awk '/^[0-9a-f]+ <benchmark_barrier/{found=1} found{print} /^$/{if(found)exit}' \
  | head -60

echo ""
echo "── GOMP_barrier symbol summary ────────────────────────────────────────"
# Count how many call sites reference GOMP_barrier (should be exactly 1:
# the single '#pragma omp barrier' in the inner loop).
echo -n "  call sites to GOMP_barrier in binary: "
objdump -d -C "$BIN" | grep -c "GOMP_barrier" || true

echo ""
echo "── GOMP_parallel symbol summary (parallel region) ─────────────────────"
echo -n "  call sites to GOMP_parallel in binary: "
objdump -d -C "$BIN" | grep -c "GOMP_parallel" || true

echo ""
echo "── Atomic increment (benchmark_atomic) ─────────────────────────────────"
# The '#pragma omp atomic counter++' must compile to a hardware 'lock' prefix
# instruction (lock add / lock xadd).  Show the benchmark_atomic function.
objdump -d -C "$BIN" \
  | awk '/^[0-9a-f]+ <benchmark_atomic/{found=1} found{print} /^$/{if(found)exit}' \
  | head -60

echo ""
echo "── lock-prefix instructions in binary ──────────────────────────────────"
echo -n "  total 'lock ' instructions: "
objdump -d -C "$BIN" | grep -c "lock " || true
echo "── End of disassembly analysis ─────────────────────────────────────────"
echo ""

# Run
echo "Running: region_reps=$REGION_REPS, barrier_reps=$BARRIER_REPS, atomic_reps=$ATOMIC_REPS, trials=$TRIALS..."

$BIN "${THREADS_LIST[0]}" $REGION_REPS $BARRIER_REPS $ATOMIC_REPS $TRIALS --csv --warmup > "$OUT"

for t in "${THREADS_LIST[@]:1}"; do
    $BIN "$t" $REGION_REPS $BARRIER_REPS $ATOMIC_REPS $TRIALS --csv --warmup | tail -n +2 >> "$OUT"
done

echo "Saved results to $OUT"
