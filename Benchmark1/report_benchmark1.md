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
| Machine | NYU CIMS crunchy node (Linux) |
| Thread counts tested | 1, 2, 4, 8 |
| Repetitions per metric | 100,000 |
| Trials per configuration | 5 |
| Compiler flags (OpenMP) | `-O3 -fopenmp -std=c++17` |
| Compiler flags (Rust) | `cargo build --release` |

---

## 4. Results

All values below are the mean across 5 trials. Units are microseconds (µs) unless stated otherwise.

### 4.1 Parallel Region Entry/Exit Overhead (µs per region)

| Threads | OpenMP | Rust | Rust / OpenMP |
|---------|-------:|-----:|--------------:|
| 1 | 2.57 | 5.54 | 2.2x |
| 2 | 6.59 | 35.98 | **5.5x** |
| 4 | 6.37 | 71.06 | **11.2x** |
| 8 | 8.91 | 144.6 | **16.2x** |

> Note: Rust trial 1 at 1 thread was 12.4 µs due to a first-run warmup spike. Trials 2–5 converged to ~3.6 µs. The mean includes the warmup trial; excluding it gives ~3.8 µs.

### 4.2 Barrier Synchronization Overhead (µs per barrier)

| Threads | OpenMP | Rust | Rust / OpenMP |
|---------|-------:|-----:|--------------:|
| 1 | 1.96 | 1.92 | ~1.0x (equal) |
| 2 | 3.41 | 16.11 | **4.7x** |
| 4 | 2.21 | 28.21 | **12.7x** |
| 8 | 4.03 | 55.47 | **13.8x** |

### 4.3 Atomic Increment Overhead (ns per increment)

| Threads | OpenMP | Rust |
|---------|-------:|-----:|
| 1 | 24 ns | 26 ns |
| 2 | 100 ns | 87 ns |
| 4 | 78 ns | 85 ns |
| 8 | 102 ns | 78 ns |

---

## 5. Analysis

### 5.1 Parallel Region: OpenMP is significantly faster and scales better

OpenMP's parallel region overhead grows only modestly from 1 to 8 threads (2.57 → 8.91 µs, a 3.5× increase), while Rust's overhead grows approximately linearly with thread count (3.6 → 144.6 µs, a ~40× increase).

This difference is explained by the **thread wake-up mechanism**:

- **OpenMP** uses **spin-waiting** between parallel regions. Worker threads busy-loop on a flag rather than sleeping. When the main thread enters a new `#pragma omp parallel`, workers are already running and can respond within nanoseconds. The cost is nearly constant regardless of thread count because all threads are already awake.

- **Rust's `std::sync::Barrier`** uses a **mutex + condvar** internally. Workers actually sleep between regions. Waking them requires an OS syscall and a context switch per thread, each costing several microseconds. With more threads, more wake-ups are needed, so the cost scales linearly.

### 5.2 Barrier: Equal at 1 thread, large gap otherwise

At 1 thread both systems measure approximately the same barrier cost (~1.9 µs), because with a single participant there is no cross-thread synchronization — the barrier returns immediately. As soon as more threads participate, Rust's condvar-based barrier degrades sharply for the same reason as above.

OpenMP's barrier cost stays almost flat (2–4 µs) because threads are already spinning and simply need to coordinate a shared counter.

### 5.3 Atomic: Essentially identical

Both implementations ultimately compile down to the same x86 `lock xadd` instruction (or equivalent). The ~10–20 ns variation across thread counts is within measurement noise and is explained by cache line contention, not by any language-level difference. This serves as a useful **control metric** — it confirms that both benchmarks are running on the same hardware under comparable conditions.

### 5.4 OpenMP outlier spikes

Several OpenMP trials have anomalously high values (e.g., trial 4 at 2 threads: 10.3 µs; trial 4 at 8 threads: 16.6 µs). These are consistent with **OS preemption events** interrupting a spinning thread. This is a known cost of spin-waiting: when the OS preempts a spinning thread the entire parallel region stalls until the thread is rescheduled. This does not affect the median behavior but does increase variance.

### 5.5 Programmer control observation

During implementation, it became clear that **OpenMP does not expose thread lifecycle control to the programmer**. There is no standard API to force OpenMP to destroy and recreate threads between regions — the thread pool reuse is internal and mandatory. This is a concrete limitation under the **programmer control** dimension: the programmer cannot choose between thread creation and thread reuse strategies in OpenMP. Rust with `std::thread` gives full control — the programmer decides exactly when threads are created, how they wait, and when they terminate.

---

## 6. Programmability Comparison

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

## 7. Summary

| Metric | Winner | Notes |
|---|---|---|
| Parallel region overhead | **OpenMP** | 5.5×–16× faster due to spin-waiting |
| Barrier overhead | **OpenMP** | 4.7×–13.8× faster due to spin-waiting |
| Atomic overhead | **Tie** | Both use the same hardware instruction |
| Lines of code | **OpenMP** | 218 vs 313 |
| Programmer control over thread lifecycle | **Rust** | OpenMP hides thread lifecycle; it cannot be controlled |
| Compile-time safety | **Rust** | Borrow checker prevents data races statically |

**Bottom line:** For workloads where parallel regions are entered and exited at high frequency, OpenMP's runtime has a large performance advantage because of its spin-based thread pool. Rust's `std::sync::Barrier` is a general-purpose primitive optimized for CPU efficiency rather than low latency. For use cases where parallel region overhead is not the bottleneck (e.g., coarse-grained workloads with large parallel bodies), this gap is not meaningful. The appropriate benchmark for evaluating that regime is Benchmark 2 (Monte Carlo Pi) and Benchmark 3 (Histogram).
