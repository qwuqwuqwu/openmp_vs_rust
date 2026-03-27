# Benchmark 2: Monte Carlo Pi — Report

## Status

**OpenMP version: complete.**
**Rust version: pending.**
**Clean 1-thread and 2-thread baseline: pending (see backlog in README.md).**

---

## 1. Purpose

This benchmark measures **scalability and raw parallel performance** on an embarrassingly parallel workload. Unlike Benchmark 1, which measured overhead with no real work, Benchmark 2 measures how well each language scales when threads are doing substantial independent compute and synchronization is nearly absent.

The only shared-state operation is a single reduction of hit counts at the end of the parallel section. Everything else is fully independent per thread.

---

## 2. Algorithm — Monte Carlo Pi Estimation

Randomly throw darts at the unit square [0, 1) × [0, 1). Count how many land inside the unit circle (x² + y² < 1.0). The ratio converges to π/4:

```
π ≈ 4 × hits / total_samples
```

Each dart throw is independent of all others. There is no shared state during sampling — threads only combine results once at the very end. This makes it the best-case scenario for parallel scaling.

---

## 3. Implementation Summary

### 3.1 OpenMP (C++)

| Property | Detail |
|---|---|
| Source file | `Benchmark2/cpp/openmp_benchmark2.cpp` |
| Compiler | `g++ -O3 -fopenmp -std=c++17` |
| Lines of code | 130 |
| Parallel construct | `#pragma omp parallel reduction(+:hits)` with `#pragma omp for schedule(static)` |

Each thread gets a private `mt19937_64` RNG seeded with `42 + thread_id`. This avoids any contention on a shared generator and makes results reproducible per thread count. The `reduction(+:hits)` clause handles combining per-thread hit counts automatically.

`schedule(static)` divides loop iterations into equal contiguous chunks at the start. This is the correct choice for uniform-cost iterations — no runtime work queue is needed.

### 3.2 Rust (std::thread)

*Pending implementation.*

---

## 4. Experimental Setup

| Parameter | Value |
|---|---|
| Machines | NYU CIMS crunchy2, crunchy5 |
| Thread counts tested | 1, 2, 4, 8, 16, 32 |
| Samples per trial | 100,000,000 |
| Trials per configuration | 5 |
| Warmup | 1 trial at 1,000,000 samples before each thread count |
| Compiler flags | `-O3 -fopenmp -std=c++17` |

---

## 5. Results

### 5.1 Raw Data — crunchy2

| Threads | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 |
|---------|---------|---------|---------|---------|---------|
| 1  | 7.871s | **31.240s** | 10.985s | 4.357s | 4.958s |
| 2  | 12.604s | 6.659s | **15.870s** | 7.783s | 4.570s |
| 4  | 1.098s | 1.101s | 1.096s | 1.090s | 1.578s |
| 8  | 0.784s | 0.581s | 0.648s | 0.646s | 0.635s |
| 16 | 0.339s | 0.309s | 0.278s | 0.283s | 0.291s |
| 32 | 0.150s | 0.152s | 0.158s | **1.810s** | **0.751s** |

### 5.2 Raw Data — crunchy5

| Threads | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 |
|---------|---------|---------|---------|---------|---------|
| 1  | **20.379s** | 4.329s | 4.329s | 5.605s | 5.469s |
| 2  | **24.413s** | 4.558s | **15.728s** | **17.849s** | **10.001s** |
| 4  | 1.372s | 1.121s | 1.089s | 1.090s | 1.090s |
| 8  | 0.544s | 0.557s | 0.543s | 0.552s | 0.563s |
| 16 | 0.274s | 0.281s | 0.290s | 0.286s | 0.290s |
| 32 | 0.147s | 0.143s | 0.146s | 0.146s | 0.147s |

---

## 6. Node Interference Phenomenon

### 6.1 What was observed

At 1 and 2 threads, the runtime variance across trials is extreme and inconsistent with the compute workload. On crunchy2, the 1-thread trials range from 4.36s to 31.24s — a 7× spread for identical work on identical hardware. On crunchy5, the 2-thread results range from 4.56s to 24.41s across five trials.

At 4 threads and above, the variance collapses. On crunchy5, all five 32-thread trials fall within 0.143–0.147s — a spread of under 3%.

### 6.2 Why this happens

Crunchy2 and crunchy5 are shared cluster nodes. When a benchmark runs with only 1 or 2 threads, it occupies 1–2 of the 64 available cores. The remaining 62–63 cores are idle and available for the job scheduler to assign other users' jobs. If another user's process lands on the same cores during the benchmark, it steals CPU time and inflates the runtime.

At 4+ threads the benchmark occupies enough cores that there is less opportunity for interference, and the OS scheduler is less likely to preempt the benchmark's threads mid-run. At 32 threads, the benchmark is using half the node, leaving little room for interference.

### 6.3 Why this is a problem

All speedup calculations are defined relative to the 1-thread runtime:

```
Speedup(n) = T(1) / T(n)
```

If T(1) is inflated by interference, every speedup number is artificially high. For example, if the true 1-thread time is 4.33s but the measured value is 20.38s due to node load, the calculated speedup at 32 threads becomes:

```
Speedup = 20.38 / 0.146 = 139×   ← meaningless
```

instead of:

```
Speedup = 4.33 / 0.146 = 29.7×   ← meaningful
```

### 6.4 Best available baseline and provisional speedup

Using only the clean trials at 1 thread (crunchy5 trials 2–3, ~4.33s):

| Threads | Avg time (crunchy5) | Speedup | Ideal speedup | Efficiency |
|---------|---------------------|---------|---------------|------------|
| 1  | 4.33s *(trials 2–3 only)* | 1.0× | 1× | 100% |
| 4  | 1.10s | **3.94×** | 4× | 98.5% |
| 8  | 0.552s | **7.85×** | 8× | 98.1% |
| 16 | 0.284s | **15.2×** | 16× | 95.3% |
| 32 | 0.146s | **29.7×** | 32× | 92.6% |

> **These numbers are provisional.** The 1-thread baseline must be confirmed
> with a clean run before these speedup values are used in final analysis.
> See backlog in README.md.

### 6.5 Mitigation going forward

Re-run the 1-thread and 2-thread configurations during off-peak hours (early morning) when the node is likely to be quiet. Five consecutive trials with low variance (within ~10% of each other) will confirm a reliable baseline. If variance remains high, consider pinning the process to specific cores using `taskset` to reduce scheduler interference.

---

## 7. Observations on Scaling

Despite the baseline uncertainty, the 4–32 thread results tell a clear story:

**Near-linear scaling all the way to 32 threads.** Efficiency stays above 92% even at 32 threads across 4 NUMA nodes. This confirms the expectation from the benchmark design: Monte Carlo Pi is so compute-bound with so little synchronization that NUMA memory latency and cross-socket traffic barely affect total runtime.

This is important context for the decision flowchart. It establishes that for embarrassingly parallel, compute-bound workloads, both OpenMP and Rust (once implemented) are expected to scale near-linearly. The large overhead gap seen in Benchmark 1 should not appear here — because there is almost nothing to be overhead of.

---

## 8. Pi Estimate Validity

All trials produce pi estimates within ±0.0002 of the true value (3.14159265...). The estimate changes between thread counts because each thread count uses a different combination of per-thread RNG seeds, producing a different set of samples. This is expected and correct — the benchmark is not designed to produce the same estimate regardless of thread count.

| Threads | Pi estimate | Error |
|---------|-------------|-------|
| 1  | 3.141452520 | −0.000140 |
| 2  | 3.141640840 | +0.000048 |
| 4  | 3.141479520 | −0.000113 |
| 8  | 3.141624560 | +0.000032 |
| 16 | 3.141544320 | −0.000048 |
| 32 | 3.141392960 | −0.000200 |

All values are within the expected statistical error for 100M samples (theoretical std dev ≈ 0.00016).

---

## 9. Next Steps

- [ ] Re-run 1-thread and 2-thread configurations on a quiet node to get a clean baseline
- [ ] Implement Rust version
- [ ] Run Rust version on crunchy and collect results
- [ ] Produce side-by-side speedup comparison between OpenMP and Rust
- [ ] Update decision flowchart Q6 with confirmed results
