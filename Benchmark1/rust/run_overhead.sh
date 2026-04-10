#!/bin/bash

set -e

BIN="./target/release/rust_benchmark1"
OUT="../results_rust.csv"

REGION_REPS=100000
BARRIER_REPS=100000
ATOMIC_REPS=100000
TRIALS=5

THREADS_LIST=(1 2 4 8 16 32)

# Build
echo "Building..."
cargo build --release 2>&1
echo "Done. Binary: $BIN"
echo ""

# ── Disassembly analysis ─────────────────────────────────────────────────────
DISASM_FILE="../disasm_rust.txt"
echo "=== Disassembly: $BIN → $DISASM_FILE ==="
objdump -d -C "$BIN" > "$DISASM_FILE"
echo "Full disasm saved."

echo ""
echo "── Rust Barrier::wait call sites ───────────────────────────────────────"
# std::sync::Barrier::wait() is the Rust equivalent of '#pragma omp barrier'.
# It internally uses a Condvar (mutex + futex).  We look for both the Rust
# symbol and the underlying syscall wrapper.
echo -n "  Barrier::wait call sites in binary: "
objdump -d -C "$BIN" | grep -c "Barrier.*wait\|barrier.*wait" || true

echo ""
echo "── futex / syscall path (Condvar wake) ─────────────────────────────────"
# The Condvar in std::sync::Barrier ultimately calls the Linux futex syscall.
# On x86-64 this appears as 'syscall' or a call to a futex wrapper symbol.
echo -n "  'syscall' instructions in binary: "
objdump -d -C "$BIN" | grep -c "syscall" || true
echo -n "  futex symbol references: "
objdump -d -C "$BIN" | grep -c -i "futex" || true

echo ""
echo "── Barrier::wait function body ─────────────────────────────────────────"
# Show the demangled Barrier::wait function to confirm it uses Condvar paths.
objdump -d -C "$BIN" \
  | awk '/std::sync::barrier::Barrier::wait/{found=1} found{print} /^$/{if(found)exit}' \
  | head -80

echo ""
echo "── lock-prefix instructions (atomic ops) ───────────────────────────────"
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
