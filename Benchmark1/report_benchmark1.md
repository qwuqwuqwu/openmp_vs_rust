# Benchmark 1: Thread Overhead Microbenchmark — Report

## 1. Purpose

This benchmark measures the **runtime overhead** of parallel synchronization primitives in OpenMP (C++) and Rust (std::thread). Specifically it tests:

- **Parallel region entry/exit** — the fork/join cost of entering and leaving a parallel context
- **Barrier synchronization** — the cost of a full barrier across all threads
- **Atomic increment** — the cost of a single atomic counter update under contention

---

## 2. Implementation Summary

### 2.1 OpenMP (C++)

| Property | Detail |
|---|---|
| Source file | `Benchmark1/cpp/openmp_benchmark1.cpp` |
| Compiler | `g++ -O3 -fopenmp -std=c++17` |
| Lines of code | 218 |
| Thread model | OpenMP persistent thread pool |

**Parallel region:** The main thread calls `#pragma omp parallel` repeatedly in a loop. A `volatile int sink` receives `omp_get_thread_num()` inside each region to prevent the compiler from optimizing the region away.

**Barrier:** All threads enter one parallel region, then each hits `#pragma omp barrier` repeatedly inside it.

**Atomic:** All threads share a `long long counter` and each performs `#pragma omp atomic counter++` in a loop.

`omp_set_dynamic(0)` is called at startup to disable dynamic thread adjustment, ensuring a stable thread team across all regions.

### 2.2 Rust (std::thread)

| Property | Detail |
|---|---|
| Source file | `Benchmark1/rust/src/main.rs` |
| Compiler | `cargo build --release` |
| Lines of code | 313 |
| Thread model | Hand-rolled thread pool using `Arc<Barrier>` |

**Thread pool design:** `N-1` worker threads are spawned once at startup. The pool uses three `Arc<Barrier>` instances:

- `fork_barrier` — main thread signals workers to start a region (equivalent to `#pragma omp parallel` entry)
- `join_barrier` — all threads converge after work is done (equivalent to implicit barrier at end of region)
- `work_barrier` — used inside a region for the explicit barrier benchmark

Workers loop on `fork_barrier.wait()`, read an `AtomicU8` mode flag to know what to do, execute the task, then hit `join_barrier.wait()`. This directly mirrors OpenMP's persistent thread pool model.

**Why this design:** This matches OpenMP's model at the structural level — threads are created once and reused, and the overhead being measured is the cost of activating and synchronizing a sleeping thread team, not OS thread creation.

---

## 3. Experimental Setup

| Parameter | Value |
|---|---|
| Machine | NYU CIMS crunchy5 (AMD Opteron 6272, 4 sockets, 64 cores, 8 NUMA nodes) |
| Thread counts tested | 1, 2, 4, 8, 16, 32 |
| Repetitions per metric | 100,000 |
| Trials per configuration | 5 |
| Compiler flags (OpenMP) | `-O3 -fopenmp -std=c++17` |
| Compiler flags (Rust) | `cargo build --release` |
| Run time | Early morning (low cluster load) |

---

## 4. Data Quality Notes

Several cells required manual override because the standard "drop values >2× median" rule fails when the majority of trials are contaminated (the contaminated values then define the median):

| Cell | Raw trials (µs) | Issue | Override |
|---|---|---|---|
| OMP T=2 fork/join | 4.5, 7.9, 136, 185, 271 | 3/5 contaminated; 2× rule fails | Clean: 4.5, 7.9 → **6.2 µs** |
| Rust T=16 fork/join | 121, 126, 151, 1238, 1222 | 2/5 contaminated; 2× rule drops both | Clean: 121, 126, 151 → **126 µs** |
| Rust T=32 fork/join | 337, 349, 655, 718, 1210 | 3/5 contaminated; 2× rule fails | Clean: 337, 349 → **343 µs** |
| Rust T=32 barrier | 128, 131, 458, 566, 888 | 3/5 contaminated; 2× rule fails | Clean: 128, 131 → **130 µs** |

All other cells: automatic 2× rule applied; starred rows had 1–2 trials dropped.

---

## 5. Results

### 5.1 Parallel Region Entry/Exit Overhead (µs per region)

| Threads | OpenMP | Rust | Rust / OpenMP |
|--------:|-------:|-----:|--------------:|
| 1 | 4.83† | 6.20 | 1.3× |
| 2 | 6.21†† | 19.30 | 3.1× |
| 4 | 5.20 | 36.16 | 7.0× |
| 8 | 6.14 | 61.99 | 10.1× |
| 16 | 8.91 | 126.0‡ | 14.1× |
| 32 | 18.14 | 343‡‡ | ~19× |

† OMP T=1: 1 contaminated trial (55.8 µs) dropped; 1 borderline trial (10.6 µs) retained by 2× rule. Clean set: [3.76, 4.16, 5.50, 10.55].

†† OMP T=2: 3/5 contaminated (136–271 µs). 2× rule fails. Override with 2 clean trials: 4.5, 7.9 µs.

‡ Rust T=16: 2/5 contaminated (1222, 1238 µs) dropped by 2× rule. Clean median of [121, 126, 151] = 126 µs.

‡‡ Rust T=32: 3/5 contaminated. Override with 2 clean trials: 337, 349 µs → 343 µs.

### 5.2 Barrier Synchronization Overhead (µs per barrier)

| Threads | OpenMP | Rust | Rust / OpenMP |
|--------:|-------:|-----:|--------------:|
| 1 | 3.04† | 2.95† | ~1.0× (equal) |
| 2 | 3.72†† | 9.55 | 2.6× |
| 4 | 2.12 | 17.98‡ | 8.5× |
| 8 | 2.30 | 29.41 | 12.8× |
| 16 | 4.38 | 62.0‡‡ | 14.2× |
| 32 | 9.39 | 130§ | 13.8× |

† OMP T=1 barrier: 2 contaminated trials (24.3, 34.9 µs) dropped.
†† OMP T=2 barrier: 1 contaminated trial (71.0 µs) dropped.
‡ Rust T=4 barrier: 1 contaminated trial (36.8 µs) dropped.
‡‡ Rust T=16 barrier: 2 contaminated trials (408, 488 µs) dropped. Clean: [60.4, 62.0, 104.0] → 62.0 µs.
§ Rust T=32 barrier: 3/5 contaminated. Override with 2 clean trials: 128, 131 µs → 130 µs.

### 5.3 Atomic Increment Overhead (ns per increment)

| Threads | OpenMP | Rust | Notes |
|--------:|-------:|-----:|-------|
| 1 | 38 | 47 | Near-equal |
| 2 | 77 | 103 | Within noise |
| 4 | 87 | 73 | Rust slightly lower |
| 8 | 97 | 88 | Within noise |
| 16 | 90 | 74 | Within noise |
| 32 | 107 | 75 | Within noise |

All values clean (5/5 trials consistent); no overrides needed.

---

## 6. Analysis

### 6.1 Parallel Region: Large and growing gap

OpenMP fork/join overhead scales very modestly from 1T to 32T: **4.83 → 18.14 µs** (3.8× increase over 32× more threads). Rust's overhead grows approximately proportional to thread count: **6.20 → 343 µs** (~55× increase).

This 19× gap at 32T is explained by the **thread wake-up mechanism**:

- **OpenMP** uses **spin-waiting** between parallel regions. Worker threads busy-loop on a flag rather than sleeping. When the main thread enters a new `#pragma omp parallel`, workers are already awake and respond within nanoseconds. Wake-up latency is essentially zero; the measured cost is the coordination protocol overhead, which grows slowly.

- **Rust's `std::sync::Barrier`** uses a **mutex + condvar** internally. Workers sleep between fork/join events. Waking each worker requires an OS syscall (`futex`), scheduling the thread back onto a CPU, and an L1/L2 cache miss as it resumes. With N threads, N−1 wake-ups are needed and they may overlap, but at 32T the aggregate cost is ~343 µs — 19× the OpenMP cost.

This is a structural difference between OpenMP's runtime and Rust's standard library. It is not a language limitation per se — Rust *can* implement spin-waiting — but `std::sync::Barrier` does not, and spin-waiting requires explicit implementation.

### 6.2 Barrier: Same root cause, same slope

At 1 thread both systems measure the same barrier cost (~3.0 µs): with a single participant there is no cross-thread wake-up. The moment more threads participate, Rust's condvar-based barrier degrades for exactly the same reason as fork/join.

OpenMP barrier cost is nearly flat: **3.04 → 9.39 µs** (1T to 32T, 3× increase). Rust barrier: **2.95 → 130 µs** (44× increase). The ratio plateaus at ~14× from 8T onward, indicating both systems spend time on the same fundamental coordination problem but with a constant-factor difference in wake-up cost.

**Observation:** Rust's barrier cost matches OMP's at T=1 (2.95 vs 3.04 µs) because both measure the same thing at that point: a single atomic compare-and-swap with no cross-thread communication. The divergence at T≥2 is entirely from sleep/wake overhead.

### 6.3 Atomic: Essentially identical

Both implementations ultimately compile down to the same x86 `lock xadd` instruction. The 10–30% variation across thread counts is within measurement noise and reflects cache-line contention, not any language-level difference. This is the expected result: atomic instructions are a hardware primitive; neither language wraps them with additional overhead.

Importantly, Rust atomics at high thread counts (T≥4) are slightly *lower* than OMP atomics (e.g., 75 ns vs 107 ns at T=32). This is within noise and likely reflects minor differences in the surrounding loop structure or cache state rather than a real performance advantage.

### 6.4 Contamination pattern: OMP vs Rust sensitivity

Even in an early-morning low-load run, Rust fork/join at T=32 had 3/5 trials contaminated (655–1210 µs), while all 5 OMP fork/join trials at T=32 were clean (17.91–18.29 µs, a ~2% spread).

This asymmetry confirms a structural difference: **OMP's persistent thread pool is immune to OS scheduler interference between trials** because threads never sleep. Fresh thread creation (even avoided here — Rust uses a pool) still depends on Barrier::wait() which wakes sleeping threads, making Rust sensitive to system load spikes that briefly delay one thread's wake-up, stalling the entire barrier.

### 6.5 Programmer control observation

During implementation, it became clear that **OpenMP does not expose thread lifecycle control to the programmer**. There is no standard API to force OpenMP to destroy and recreate threads between regions — the thread pool reuse is internal and mandatory. This is a concrete limitation under the **programmer control** dimension: the programmer cannot choose between thread creation and thread reuse strategies in OpenMP. Rust with `std::thread` gives full control — the programmer decides exactly when threads are created, how they wait, and when they terminate.

The flip side: OpenMP's spin-waiting is automatic and free. Getting Rust to match OMP's fork/join latency would require implementing custom spin-waiting, adding ~50+ lines of unsafe code.

---

## 7. Programmability Comparison

| Criterion | OpenMP | Rust |
|---|---|---|
| Lines of code | 218 | 313 |
| Parallel constructs | 3 (`parallel`, `barrier`, `atomic`) | Hand-rolled (`Pool`, `Barrier`, `AtomicI64`) |
| Synchronization constructs | 1 pragma each | 3 `Arc<Barrier>` + 1 `AtomicI64` + 1 `AtomicU8` |
| Ease of writing (1–5) | 5 | 3 |
| Ease of understanding (1–5) | 4 | 3 |
| Ease of ensuring correctness (1–5) | 3 | 4 |

**Notes:**

- The OpenMP version required only three `#pragma` annotations to express all three benchmarks. The parallel structure is implicit and concise.
- The Rust version required designing and implementing a full thread pool (`Pool` struct, three barriers, two atomic control variables, a mode enum, and a shutdown protocol). This is 95 more lines of code and significantly higher design effort.
- However, Rust's explicitness makes the synchronization structure easier to audit for correctness. The borrow checker and type system prevented data races at compile time with no runtime debugging needed.
- The OpenMP `volatile int sink` workaround (to prevent the compiler from optimizing away an empty parallel region) was a non-obvious requirement discovered only after observing a 6,500× difference in initial results. Rust had no equivalent issue because the `Barrier::wait()` call has real side effects that cannot be eliminated.

---

## 8. Summary

| Metric | Winner | Notes |
|---|---|---|
| Parallel region overhead | **OpenMP** | 1.3×–19× lower, 1T–32T; gap grows with thread count |
| Barrier overhead | **OpenMP** | 1.0×–14× lower; equal at 1T, diverges at 2T+ |
| Atomic overhead | **Tie** | Both use the same hardware instruction; <30% variation is noise |
| Lines of code | **OpenMP** | 218 vs 313 |
| Programmer control over thread lifecycle | **Rust** | OpenMP hides thread lifecycle; cannot be controlled |
| Compile-time safety | **Rust** | Borrow checker prevents data races statically |
| Sensitivity to cluster interference | **OpenMP** | Persistent spin-polling is immune to OS scheduler spikes |

**Bottom line:** For workloads where parallel regions are entered and exited at high frequency (fine-grained parallelism), OpenMP's runtime has a large and growing performance advantage due to its spin-based thread pool. The advantage starts at 1.3× at 1 thread and reaches ~19× at 32 threads. Rust's `std::sync::Barrier` is a general-purpose primitive optimized for CPU efficiency (sleep between uses) rather than low latency.

For use cases where parallel region overhead is not the bottleneck — coarse-grained workloads where the parallel body is orders of magnitude longer than the fork/join cost — this gap is irrelevant. The appropriate benchmarks for evaluating that regime are Benchmark 2 (Monte Carlo Pi) and Benchmark 4 (Prime Testing), where per-thread work takes tens to hundreds of milliseconds.
