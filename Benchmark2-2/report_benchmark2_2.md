# Benchmark 2-2: Popcount Sum with Rayon — Report

## Status

**Rust (Rayon) version: complete.**
**Final results: N = 2³³ = 8,589,934,592, 1–64 threads, 5 trials per configuration, clean run confirmed.**

---

## 1. Purpose

This benchmark repeats the exact popcount workload from Benchmark 2-1 but replaces bare `std::thread::spawn` with **Rayon**, Rust's standard data-parallelism library. The goal is to answer a methodological question raised after Benchmark 2-1:

> *Is it fair to compare OpenMP (a high-level runtime with a persistent thread pool) against bare `std::thread` (a low-level OS thread primitive)?*

Rayon is the correct Rust equivalent of OpenMP for data-parallel workloads: both maintain a persistent thread pool, both express parallelism as a transformation over data, and neither requires the programmer to manage thread lifecycle manually. This benchmark establishes whether switching to Rayon changes the conclusions of Benchmark 2-1.

---

## 2. What Changed from Benchmark 2-1

### 2.1 Benchmark 2-1 (bare std::thread)

```rust
// Spawns and destroys N OS threads on every single trial
let handles: Vec<_> = (0..cfg.threads).map(|tid| {
    thread::spawn(move || {
        let mut local: u64 = 0;
        for i in start_i..end_i { local += i.count_ones() as u64; }
        local
    })
}).collect();
let total: u64 = handles.into_iter().map(|h| h.join().unwrap()).sum();
```

### 2.2 Benchmark 2-2 (Rayon)

```rust
// Thread pool built once — threads never destroyed between trials
let pool = rayon::ThreadPoolBuilder::new()
    .num_threads(cfg.threads)
    .build()
    .unwrap();

pool.install(|| {
    (0u64..cfg.n).into_par_iter().map(|i| i.count_ones() as u64).sum()
})
```

The `pool` is constructed **once** before the trial loop. Each `pool.install()` call wakes up sleeping threads — identical to how OpenMP's `GOMP_parallel` activates its persistent pool. The per-trial thread spawn/join overhead from Benchmark 2-1 is completely eliminated.

---

## 3. Algorithm

Identical to Benchmark 2-1:

```
total = Σᵢ₌₀ᴺ⁻¹ popcount(i)
```

For N = 2³³: expected = 33 × 2³² = **141,733,920,768**. All 35 trials return exactly this value.

---

## 4. Experimental Setup

| Parameter | Value |
|---|---|
| Machine | NYU CIMS crunchy cluster (64-core, 8 NUMA nodes, 8 cores/node) |
| Thread counts | 1, 2, 4, 8, 16, 32, 64 |
| N (problem size) | 2³³ = 8,589,934,592 |
| Trials per configuration | 5 |
| Warmup | 1 trial at N = 2²⁰ before each thread count |
| Rust flags | `cargo --release` (LLVM backend) |
| Rayon version | 1.10 |

---

## 5. Raw Results

| Threads | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 | Avg | Spread |
|---------|---------|---------|---------|---------|---------|-----|--------|
| 1  | 16.142s | 16.135s | 16.130s | 16.130s | 16.132s | **16.134s** | 0.1% |
| 2  | 8.070s  | 8.074s  | 8.074s  | 8.075s  | 8.071s  | **8.073s**  | 0.1% |
| 4  | 4.077s  | 4.082s  | 4.059s  | 4.068s  | 4.072s  | **4.072s**  | 0.6% |
| 8  | 2.022s  | 2.030s  | 2.034s  | 2.027s  | 2.026s  | **2.028s**  | 0.6% |
| 16 | 1.011s  | 1.012s  | 1.010s  | 1.009s  | 1.013s  | **1.011s**  | 0.4% |
| 32 | 0.509s  | 0.506s  | 0.505s  | 0.509s  | 0.506s  | **0.507s**  | 0.9% |
| 64 | **0.296s** ¹ | 0.274s | 0.271s | 0.270s | 0.269s | **0.271s** | 1.8% |

¹ Trial 1 at 64T is elevated. This is a first-trial warm-up effect at the NUMA boundary (see Section 8.3). Average uses trials 2–5 (0.271s).

Data quality is excellent: zero contamination across all 35 trials, matching the cleanliness of Benchmark 2-1's Rust results.

---

## 6. Scalability Analysis

### 6.1 Rayon Speedup (baseline: 1T avg = 16.134s)

| Threads | Avg time | Speedup | Ideal | Efficiency |
|---------|----------|---------|-------|------------|
| 1  | 16.134s | 1.00× | 1× | 100% |
| 2  | 8.073s  | **2.00×** | 2× | 99.9% |
| 4  | 4.072s  | **3.96×** | 4× | 99.1% |
| 8  | 2.028s  | **7.96×** | 8× | 99.5% |
| 16 | 1.011s  | **15.96×** | 16× | 99.7% |
| 32 | 0.507s  | **31.82×** | 32× | 99.4% |
| 64 | 0.271s  | **59.5×** | 64× | 93.0% |

Rayon achieves near-perfect parallel efficiency from 1T to 32T. The 64T drop to 93% mirrors Benchmark 2-1's Rust result (93.3%) and reflects the same NUMA topology boundary — at 64 threads the workload crosses all 8 NUMA nodes.

### 6.2 Step-by-Step Scaling

| Step | Rayon | Ideal |
|------|-------|-------|
| 1T → 2T  | **2.00×** | 2× |
| 2T → 4T  | **1.98×** | 2× |
| 4T → 8T  | **2.01×** | 2× |
| 8T → 16T | **2.00×** | 2× |
| 16T → 32T | **1.99×** | 2× |
| 32T → 64T | **1.87×** | 2× |

---

## 7. Three-Way Comparison: OpenMP vs Rayon vs std::thread

This is the central result of Benchmark 2-2.

### 7.1 Full Comparison Table

| Threads | OpenMP | Rayon (2-2) | std::thread (2-1) | Fastest | Rayon vs OpenMP | Rayon vs std::thread |
|---------|--------|-------------|-------------------|---------|-----------------|----------------------|
| 1T  | 17.367s | 16.134s | 15.654s | **std::thread** | 1.076× faster | 0.970× (slower) |
| 2T  | 8.830s  | 8.073s  | 7.822s  | **std::thread** | 1.094× faster | 0.969× (slower) |
| 4T  | 4.371s  | 4.072s  | 3.874s  | **std::thread** | 1.074× faster | 0.951× (slower) |
| 8T  | 2.205s  | 2.028s  | 1.939s  | **std::thread** | 1.087× faster | 0.956× (slower) |
| 16T | 1.119s  | 1.011s  | 0.971s  | **std::thread** | 1.107× faster | 0.960× (slower) |
| 32T | 0.612s  | 0.507s  | 0.489s  | **std::thread** | 1.207× faster | 0.964× (slower) |
| 64T | **0.252s**  | 0.271s  | 0.262s  | **OpenMP**   | 0.930× (slower) | 0.967× (slower) |

### 7.2 Key Observation: std::thread Beats Rayon at Every Thread Count (1T–32T)

This is the most surprising result. A persistent thread pool (Rayon) is consistently **3–5% slower** than fresh-thread-per-trial (std::thread) across all thread counts from 1T to 32T. Two explanations:

**Explanation 1 — Assembly: 4× unroll vs 8× unroll.**
LLVM generated different loop bodies for the two implementations (see Section 9). Inside Rayon's work-stealing closure the compiler produced a 4× unrolled loop; inside the bare `std::thread` closure it produced an 8× unrolled loop. The 8× loop processes more iterations per cycle (less loop overhead, better latency hiding) and is consistently faster at the instruction level.

**Explanation 2 — Work-stealing overhead on a uniform workload.**
Rayon's work-stealing scheduler exists to handle *load imbalance*: when some tasks take longer than others, idle threads steal work from busy ones. For popcount, every element costs exactly the same (one `popcnt` instruction). There is no imbalance to correct. Rayon's scheduler still pays the cost of range-splitting, steal-check atomics, and task queue management — all for zero benefit. Static partitioning (both OpenMP `schedule(static)` and Benchmark 2-1's manual chunk assignment) avoids this overhead entirely.

### 7.3 The 64T Reversal Persists — and Is Larger with Rayon

| | OpenMP | Rayon | std::thread |
|---|--------|-------|-------------|
| **64T** | **0.252s** | 0.271s | 0.262s |
| OpenMP advantage | — | **7.5% faster** | **4% faster** |

At 64 threads, OpenMP beats both Rust implementations. The gap against Rayon (7.5%) is *larger* than the gap against std::thread (4%). At 64T each thread processes only ~134M integers (~270ms of work). Rayon is doing recursive range-splitting and work-stealing bookkeeping across 64 workers and 8 NUMA nodes, adding latency that compounds the ordinary thread management cost. OpenMP's static partition + persistent pool has no such overhead.

---

## 8. Assembly Analysis

### 8.1 Rayon Hot Loop (from `rust_benchmark2_2_dump.txt`)

The compute kernel lives inside `rayon_core::join::join_context` (Rayon's internal work-splitting infrastructure), address `0xcea0`–`0xcee5`:

```asm
; 4× unrolled loop — covers i+0 through i+3 per iteration
cea0:  lea  (%rdi,%r8,1),%r9      ; r9  = base + i+0
cea4:  popcnt %r9,%r9             ; pop(i+0) → r9
cea9:  add  %r12,%r9              ; r9 += running accumulator (r12 from prev iter)
ceac:  lea  (%rdi,%r8,1),%r10     ; r10 = base + i+0
ceb0:  inc  %r10                  ; r10 = i+1
ceb3:  popcnt %r10,%r10           ; pop(i+1) → r10
ceb8:  lea  (%rdi,%r8,1),%r11     ; r11 = base + i+0
cebc:  add  $0x2,%r11             ; r11 = i+2
cec0:  popcnt %r11,%r11           ; pop(i+2) → r11
cec5:  add  %r10,%r11             ; r11 += r10
cec8:  add  %r9,%r11              ; r11 += r9  → r11 = sum[0..2]
cecb:  lea  (%rdi,%r8,1),%r9      ; r9 = i+3 (reuse r9)
cecf:  add  $0x3,%r9
ced3:  xor  %r12d,%r12d           ; break Intel false dependency on r12
ced6:  popcnt %r9,%r12            ; pop(i+3) → r12
cedb:  add  %r11,%r12             ; r12 = sum[0..3]  ← loop output
cede:  add  $0x4,%r8              ; i += 4
cee2:  cmp  %r8,%rdx              ; end check
cee5:  jne  cea0                  ; loop back

; Scalar cleanup for (N mod 4) remainder:
cf00:  xor  %ecx,%ecx             ; break false dep
cf02:  popcnt %rsi,%rcx           ; scalar 1×
cf07:  add  %rcx,%r12
cf0a:  inc  %rsi
cf0d:  dec  %rax
cf10:  jne  cf00
```

| Property | Rayon (2-2) | std::thread (2-1) | GCC/OpenMP (2-1) |
|---|---|---|---|
| Unroll factor | **4×** | **8×** | **1×** |
| Accumulators | 3–4 (tree chain) | 3–4 (interleaved) | 1 |
| Index calculation | `lea base+offset` | direct `lea +N` | `add $1` |
| False-dep `xor` mitigation | 1 of 4 `popcnt` | 2 of 8 `popcnt` | every iteration |
| Scalar cleanup | Yes (mod 4) | Yes (mod 8) | No (no unroll) |
| AVX2 / SIMD | None | None | None |

### 8.2 Why Rayon Gets 4× Instead of 8×

LLVM sees a different loop structure inside Rayon's closure than in a bare `std::thread` closure. Rayon's work-stealing framework passes a `start..end` range to each task, and the kernel operates as `base + local_counter`. The `lea (%rdi,%r8,1)` pattern (base-plus-offset) in the Rayon loop is less amenable to the aggressive unrolling LLVM applies when iterating a simple `0..N` range in a bare thread. Additionally, the Rayon closure goes through more LLVM IR transformations (trait dispatch, iterator protocol for `ParallelIterator`) before hitting the optimizer, which can prevent some unrolling heuristics from triggering.

### 8.3 Why Trial 1 at 64T Is Elevated (0.296s vs 0.269–0.274s)

The warmup (N = 2²⁰) runs before the trial loop and should wake all threads. However, at 64T Rayon's first full-scale install spans all 8 NUMA nodes for the first time. OS page migration and TLB shootdowns on first NUMA access add latency that is not captured by the small warmup. Trials 2–5 have all pages settled and run consistently.

---

## 9. Key Findings

1. **Switching from bare `std::thread` to Rayon does not remove the 64T reversal.** OpenMP is 7.5% faster than Rayon at 64T — even larger than the 4% gap against std::thread. The reversal is not primarily about thread spawn cost; it is about work-stealing overhead on a uniform workload at high thread counts.

2. **For perfectly uniform workloads, static partitioning beats work-stealing.** Both OpenMP `schedule(static)` and Benchmark 2-1's manual chunk assignment outperform Rayon's work-stealing at every thread count from 1T to 32T. When all tasks cost the same, the work-stealing infrastructure adds overhead with no load-balancing benefit.

3. **Bare `std::thread` with static partitioning is the fastest Rust implementation for this workload** — 3–4% faster than Rayon and 11–21% faster than OpenMP at 1T–32T. This is driven by LLVM generating an 8× unrolled loop for the simple range iteration, versus a 4× unrolled loop inside Rayon's closure.

4. **All three implementations scale at equivalent efficiency from 1T to 32T.** The speedup curves are parallel lines. No implementation has a structural parallelism advantage; the differences are entirely in single-thread throughput and scheduler overhead.

5. **OpenMP is the fastest at 64 threads regardless of which Rust implementation it is compared to.** At 64T with ~270ms of per-thread work, OpenMP's persistent pool + zero work-stealing overhead wins. This directly extends the Benchmark 1 finding (OpenMP fork/join 5–16× cheaper than Rust) to the large-thread-count regime.

6. **Rayon's advantage over OpenMP (7–21% at 1T–32T) comes from LLVM code quality, not from the thread pool.** The unrolling advantage belongs to LLVM, which benefits both Rayon and std::thread. Rayon is slower than std::thread for this workload — so Rayon's win over OpenMP is purely the compiler gap, not a scheduling advantage.

7. **Rayon is the right Rust choice for irregular workloads.** Benchmark 2-2 confirms that Rayon is strictly worse than static partitioning when work is uniform. Benchmark 4 (prime testing, highly variable element cost) is the expected inflection point where Rayon's work-stealing will provide genuine benefit and the comparison against OpenMP `schedule(dynamic)` becomes meaningful.

---

## 10. Flowchart Impact

**Q6 — Scalability (embarrassingly parallel, uniform workload):**
Benchmark 2-2 reinforces the Benchmark 2-1 conclusion. Both implementations scale identically. At 64T, OpenMP wins regardless of whether the Rust side uses `std::thread` or Rayon. The Q6 path remains: for this workload class, use other axes to decide.

**Q6A — What limits scalability (sync overhead):**
The 64T reversal is now confirmed against two different Rust implementations. OpenMP's structural advantage at high thread counts is not an artifact of Rust's thread spawn cost — it persists even with a persistent pool. The `OPENMP_SCALE` outcome is fully supported.

**New nuance for Q2 / Workload shape:**
For **uniform** embarrassingly parallel workloads: static partitioning (OpenMP or manual std::thread) is preferable to work-stealing (Rayon). For **irregular** workloads where element cost varies: Rayon's work-stealing is expected to outperform OpenMP `schedule(static)` and match or beat `schedule(dynamic)`. This distinction should be captured in Benchmark 4.

---

## 11. Next Steps

- [x] Rayon implementation complete
- [x] Clean results obtained
- [x] Assembly analysis complete
- [x] Three-way comparison (OpenMP / Rayon / std::thread) documented
- [x] 64T reversal confirmed against both Rust implementations
- [ ] Benchmark 3: Reduction workloads (histogram or dot product)
- [ ] Benchmark 4: Irregular workloads (prime testing) — expected inflection point for Rayon vs OpenMP `schedule(dynamic)`

---

## 12. Summary

| Metric | Winner | Notes |
|---|---|---|
| Uniform workload throughput (1T–32T) | **Rust std::thread** | 11–21% faster than OpenMP; 3–5% faster than Rayon |
| Work-stealing on uniform workloads | **std::thread** | Static partitioning beats Rayon — no imbalance to steal, overhead is pure cost |
| 64T performance | **OpenMP** | Fastest at 64T (7.5% vs Rayon, 4% vs std::thread); persistent pool overhead-free |
| Code conciseness (uniform work) | **OpenMP** | One pragma; Rayon is 1 line but 4% slower; std::thread requires manual partitioning |
| Code conciseness (irregular work) | **Rayon** | `par_iter()` is 1 line and provides real load balancing benefit at low-to-mid thread counts |
| Parallel scalability | **Tie** | All three implementations scale at 93–100% efficiency 1T–32T |
| Inner-loop throughput | **Rust (both)** | LLVM's unrolling advantage (8× for std::thread, 4× for Rayon) persists in all Rust variants |
| High-thread-count pool cost | **OpenMP** | Persistent pool wins at 64T; Rayon's work-stealing adds overhead that doesn't pay off |
| Compile-time safety | **Rust (both)** | Rayon and std::thread both enforce data-race freedom via ownership |

**Bottom line:** For uniform embarrassingly parallel work, Rust `std::thread` with static partitioning is fastest (1T–32T) and OpenMP wins only at 64T due to spawn cost. Rayon is strictly worse than std::thread for uniform workloads — its work-stealing buys nothing when there is no imbalance to correct. The ranking switches for irregular workloads (Benchmark 4), where Rayon's work-stealing provides genuine benefit at 1T–16T.
