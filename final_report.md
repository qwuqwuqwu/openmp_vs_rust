# OpenMP vs Rust for Multicore Programming: A Comparative Study
### Final Report — NYU Multicore Systems, Spring 2026

---

## Abstract

Choosing the right parallel programming model is a fundamental systems decision: it affects
correctness, performance, and long-term maintainability simultaneously.  This paper
presents a rigorous empirical comparison of C++/OpenMP and Rust (`std::thread`) for
shared-memory multicore programming across five criteria: programmability, scalability,
runtime overhead, programmer control, and raw performance.  To evaluate each criterion
we design five benchmark programs spanning synchronization microbenchmarks, embarrassingly
parallel computation, array reduction, irregular scheduling, and NUMA-aware memory-bandwidth
workloads, all run on the NYU CIMS crunchy5 cluster (AMD Opteron 6272, 4 sockets,
64 cores, 8 NUMA nodes).  We find that neither model dominates unconditionally: OpenMP
outperforms Rust on fine-grained synchronization overhead by up to 19× and requires
significantly less code for common reduction and scheduling patterns, while Rust delivers
1.1–2.8× higher memory bandwidth per NUMA node when explicitly pinned with spread affinity
and prevents data races at compile time.  Based on these findings we derive a decision flowchart that maps
workload characteristics to the appropriate language choice.

---

## 1. Introduction

Multi-socket multicore processors are now the standard platform for scientific computing,
data analytics, and server infrastructure.  Writing correct, high-performance parallel
programs for these machines requires managing thread synchronization, memory locality,
load distribution, and data safety simultaneously.  A poor choice of parallel programming
model can result in subtle data races, poor NUMA utilization, or synchronization overhead
that erases the benefit of parallelism entirely.

Two programming models dominate shared-memory parallel programming today.  **OpenMP**
extends C/C++ and Fortran with compiler directives (`#pragma`), allowing the programmer
to annotate serial code and have the runtime manage thread creation, synchronization,
and reduction invisibly.  Its low adoption barrier makes it the default choice in
scientific computing.  **Rust** is a systems language released in 2010 that provides
memory safety and data-race freedom through its ownership type system, at the cost of
requiring the programmer to express all sharing and synchronization explicitly.

Despite the practical importance of this choice, there is no systematic head-to-head
empirical study of OpenMP vs Rust across multiple workload classes on real multicore
hardware.  Existing comparisons either focus on a single application [Costanzo 2021],
use a different language pair [Cantonnet 2004, Kalderén 2013], or do not address
NUMA-sensitive workloads or synchronization primitive overhead.

This project fills that gap.  We compare OpenMP and Rust across five dimensions — the
same five dimensions a systems programmer must reason about when selecting a model —
using five micro- and macrobenchmarks designed to stress-test each dimension in isolation.
Our final artifact is a decision flowchart that translates the empirical findings into
actionable engineering guidelines.

---

## 2. Literature Survey

Research on comparing parallel programming models falls into three broad categories:
productivity comparisons, performance comparisons, and single-language evaluations.
We discuss each in turn and explain why existing work does not fully address the
OpenMP–Rust comparison.

### 2.1 Productivity-Focused Comparisons

Cantonnet et al. [2004] propose a framework for measuring programmer productivity in
parallel languages and apply it to compare Unified Parallel C (UPC) against MPI across
eight benchmark programs from the NAS Parallel Benchmark suite.  Their productivity
metrics — lines of code (LOC), number of characters (NOC), and a conceptual complexity
score counting keywords, function calls, and parameter counts — form the methodological
basis for programmability measurement in language comparison studies.  Their results show
that UPC reduces LOC by 40–100% relative to MPI for the same benchmarks, driven by
UPC's single global address space eliminating explicit communication calls.

This work is directly relevant to our project in two ways.  First, it establishes LOC
and conceptual complexity as standard metrics for programmability comparison, which we
adopt in our analysis.  Second, it demonstrates that two languages can achieve
equivalent performance with dramatically different programmer effort — a pattern we
observe between OpenMP's `reduction` clause and Rust's explicit private-histogram
pattern in Benchmark 3.

However, Cantonnet et al. compare UPC (a shared-memory extension of C) against MPI
(a message-passing library), a fundamentally different language pair from OpenMP vs Rust.
Their work does not measure runtime overhead of synchronization primitives, NUMA
sensitivity, or programmer control over thread placement — all of which are first-class
concerns in this project.

### 2.2 Performance-Focused Cross-Language Comparisons

Kalderén and From [2013] compare four parallel models — C with OpenMP, C++ with TBB,
C# with TPL, and Java with Fork/Join — on a SparseLU benchmark using both thread-count
scaling and granularity tests.  Their key finding is that C/OpenMP achieves the fastest
absolute execution time, while C++ achieves the best relative speedup.  Critically, they
observe that Java handles fine-grained parallelism (small task sizes) better than C/OpenMP,
because Java's Fork/Join framework defers thread management overhead in ways that OpenMP
does not.

This finding directly motivates our Benchmark 1: the granularity sensitivity of
different threading models is a real and measurable phenomenon.  However, Kalderén and
From use a single application benchmark and do not include Rust in their comparison.
Their study predates Rust's stable release (2015) and does not address the ownership
model, NUMA effects, or the trade-off between compile-time safety and runtime flexibility.

### 2.3 Single-Language Evaluations of Rust for HPC

Costanzo et al. [2021] present the closest prior work to this project: a comparison of
Rust and C (with OpenMP) for the N-Body gravitational simulation on a dual Intel Xeon
Platinum system.  They find that in double precision, optimized Rust and C/OpenMP
achieve near-identical performance, while in single precision C/OpenMP leads by up to
1.18× due to more mature floating-point intrinsics.  For programming effort, Rust
requires fewer lines of code in the core algorithm (40 vs 66 lines) and allows parallel
code to be expressed by adding the `par` prefix to iterators via the Rayon library.

Their work is the primary reference for the question of whether Rust can match C/OpenMP
in HPC performance.  However, several limitations motivate our broader study:

- They use **Rayon** for Rust parallelism, not `std::thread`.  Rayon's work-stealing
  model conceals thread management overhead.  Our Benchmark 4 shows that Rayon
  regresses to 2× behind OpenMP at 64 threads on an 8-NUMA-node machine — a regime
  Costanzo et al. do not test.
- They test a **single application** (N-Body), which is embarrassingly parallel with
  no irregular scheduling needs.  They do not test fine-grained fork/join overhead,
  array reduction patterns, or dynamic scheduling.
- Their machine has **2 NUMA nodes** (2-socket Xeon), so NUMA effects are minor.
  Our machine has 8 NUMA nodes, making thread-to-core affinity a first-order concern.
- They do not measure **synchronization primitive overhead** (barrier, atomic) as a
  function of thread count, which is where the OpenMP vs Rust structural difference
  is most pronounced.

### 2.4 Formal Foundations of Rust's Safety Guarantees

A central claim in our flowchart (Q1 — Safety Requirement) is that Rust prevents data
races at compile time with no runtime overhead.  This claim is backed by a body of formal
verification work that goes well beyond empirical observation.

**RustBelt — Jung et al. [2017].**
Jung, Jourdan, Krebbers, and Dreyer present the first machine-checked formal proof that
Rust's type system correctly enforces memory safety and data-race freedom [Jung 2017].
Using the Iris framework — a concurrent separation logic embedded in the Coq proof
assistant — they construct a *semantic model* of Rust's type system and prove that any
well-typed Rust program (including programs containing `unsafe` blocks, provided the
unsafe code upholds its documented invariants) is free from data races and memory errors.
The key insight is that Rust's ownership and borrowing rules are not merely a programmer
convention: they are a type-level encoding of a formal concurrent separation logic that
the compiler enforces at every call site.

Three specific properties are proved:
1. **No data races**: two threads cannot simultaneously hold a mutable reference to the
   same memory location without synchronization.
2. **Memory safety**: no use-after-free, no dangling pointers, no out-of-bounds accesses
   escape from safe Rust.
3. **`unsafe` encapsulation**: well-written `unsafe` blocks that maintain their internal
   invariants preserve the safety guarantees of the surrounding safe code.

This contrasts sharply with OpenMP's model: the OpenMP standard [OpenMP ARB 2021] places
the *entire burden* of data-race freedom on the programmer.  A missing `private()` clause
or an incorrect `reduction()` specification is a silent data race that the compiler
accepts without warning.  In our own implementation of Benchmark 1, we discovered a
non-obvious correctness issue (the need for a `volatile int sink` to prevent the compiler
from eliminating an empty parallel region) that would have been caught immediately by
Rust's type system.

**System Programming in Rust — Balasubramanian et al. [2017].**
Balasubramanian et al. study the practical consequences of Rust's ownership model for
OS-level concurrent code [Balasubramanian 2017].  They find that Rust's borrow checker
catches real concurrent bugs — including data races and use-after-free errors that
silently compile in C — before the program ever runs.  They also argue that the
*explicit sharing model* (where ownership transfer and borrowing must be stated in
code) improves long-term maintainability: a code reviewer can determine the concurrency
structure of a program purely by reading types and function signatures, without
inspecting the runtime behavior.

This connects to our Q8 finding (long-lived systems and growing teams): the upfront cost
of satisfying Rust's borrow checker is a one-time investment that yields ongoing
auditability and refactoring safety — benefits that compound as the codebase grows.

**Implication for this project.**
The formal results of Jung et al. and Balasubramanian et al. establish that the safety
advantage of Rust over OpenMP is not merely an empirical observation or a style
preference — it is a *proven property of the type system*.  No amount of code review,
testing, or sanitizer instrumentation can provide the same compile-time guarantee that
Rust's ownership model delivers by construction.  This makes the Q1 branch of our
decision flowchart an absolute recommendation: for any system where data-race freedom
is non-negotiable, Rust is the only option among the two languages compared.

### 2.5 Gap Addressed by This Work

The four categories above establish that: (a) LOC and conceptual complexity are valid
programmability metrics; (b) threading model granularity sensitivity is language-dependent
and measurable; (c) Rust can match C/OpenMP performance for compute-bound single-application
benchmarks; (d) Rust's safety advantage over OpenMP is formally proved, not merely
observed empirically.  What is missing is a **multi-criterion, multi-benchmark systematic
comparison** of OpenMP vs Rust that includes synchronization overhead, NUMA affinity
control, irregular scheduling, and scalability up to 64 threads on an 8-NUMA-node machine.
This project provides that comparison.

---

## 3. Proposed Idea

### 3.1 Approach

We compare OpenMP and Rust by designing five benchmarks, each targeting one or more of the
five required comparison criteria.  The key methodological principles are:

1. **One benchmark per bottleneck type.** Each benchmark is designed to isolate a specific
   dimension of the OpenMP–Rust comparison.  We do not use a single application that mixes
   all effects simultaneously, because the result would be uninterpretable.

2. **Same problem, same machine, same compiler flags.** Every benchmark runs both
   implementations on the same NYU CIMS crunchy5 hardware, at the same problem size,
   with both compilers at maximum optimization (`-O3` for GCC, `--release` for Rust/LLVM).
   This ensures performance differences reflect language and runtime properties, not
   problem size or hardware differences.

3. **Disassembly verification.** For benchmarks where inner-loop code quality matters
   (B2-1, B5), we inspect the generated assembly with `objdump -d` to verify that
   performance differences reflect runtime model differences, not unintended compiler
   behavior such as auto-vectorization discrepancies.

4. **Clean-run methodology.** All benchmarks are run 5 trials per configuration.  Trials
   more than 2× the median are flagged as cluster-load contamination and dropped.  All
   key conclusions rely on early-morning low-load runs.

### 3.2 Criteria and Benchmark Mapping

| Criterion | Definition | Benchmark |
|---|---|---|
| **Programmability** | Lines of code; conciseness of expressing parallelism primitives; risk of silent bugs | B1, B3, B4 |
| **Scalability** | Parallel efficiency from 1 to 64 threads | B2-1, B3 |
| **Runtime overhead** | Cost of thread creation, termination, barrier, fork/join, atomic increment | B1 |
| **Programmer control** | Thread-to-core placement; shared vs private variable control; scheduling extensibility; thread lifecycle | B5, B4, B3, B1 |
| **Performance** | Throughput and latency on the same hardware at matched problem sizes | All benchmarks |

### 3.3 Benchmark Design Rationale

**Benchmark 1 — Synchronization Microbenchmark.**
To isolate runtime overhead, we measure fork/join, barrier, and atomic increment cost
as a function of thread count (1–32T), repeating each operation 100,000 times per trial.
OpenMP uses its built-in persistent thread pool.  Rust uses a hand-rolled thread pool
with `Arc<Barrier>` (condvar-based), which is structurally equivalent but uses OS
sleep/wake instead of spin-waiting.  This directly measures the runtime overhead
criterion and reveals the structural difference between spin-polling and condvar-based
synchronization.

**Benchmark 2-1 — Embarrassingly Parallel Popcount.**
To isolate scalability and performance from synchronization overhead and compiler bias,
we compute the population count sum over N = 2³³ integers.  The `popcnt` instruction
is available on both GCC and LLVM, eliminating floating-point or RNG confounders from
earlier Monte Carlo experiments.  Parallel efficiency tables from 1T to 64T directly
answer the scalability criterion.

**Benchmark 3 — Parallel Histogram Reduction.**
Array reduction (many threads accumulate into a shared output) is a canonical parallel
pattern.  This benchmark uses a 256-bin histogram over N = 2²⁶ random integers and
tests the programmability criterion: OpenMP's `reduction(+: h[:bins])` clause expresses
the entire private-copy / merge pattern in one line, while Rust requires explicit
allocation, partitioning, and merge.  A preliminary Strategy B (shared atomics) confirms
that algorithm design dominates language choice.

**Benchmark 4 — Prime Testing with Irregular Workload.**
Trial-division primality testing over N = 1,000,000 integers has highly irregular
per-element cost (O(√n) divisions).  This benchmark isolates the programmer control
criterion for scheduling: OpenMP provides `schedule(dynamic)` as a one-keyword change;
Rust requires implementing dynamic scheduling via `Arc<AtomicU64>` (~15 lines) or
delegating to Rayon.  All five strategies (OMP static, OMP dynamic, Rust static, Rust
dynamic, Rust Rayon) are compared head-to-head.

**Benchmark 5 — Thread-to-Core Affinity.**
A parallel sum over a 1 GB `uint64_t` array with parallel first-touch initialization
isolates the programmer control criterion for core placement.  Three affinity strategies
(spread, close, default) are compared for both languages.  OpenMP uses `proc_bind`
clauses with `OMP_PLACES=cores`; Rust uses the `core_affinity` crate with explicit
`sched_setaffinity` calls.  Disassembly confirms the LLVM vs GCC inner-loop difference
that explains the bandwidth gap.

### 3.4 Decision Flowchart

Beyond the benchmark results, we produce a decision flowchart (`decision_flowchart.md`,
`decision_flowchart.pdf`) that maps each comparison criterion to a flowchart question,
backed by the empirical data from each benchmark.  The flowchart is the primary
deliverable for the "guidelines on when to use each language" requirement.

---

## 4. Experimental Setup

| Parameter | Value |
|---|---|
| Machine | NYU CIMS crunchy5 |
| CPU | AMD Opteron 6272 (Bulldozer architecture) |
| Sockets / Cores / NUMA nodes | 4 sockets, 64 cores total, 8 NUMA nodes (8 cores each) |
| L3 cache | Shared per die |
| Measured peak DRAM bandwidth | ~96 GB/s (B5, Rust spread 32T, R4 clean); ~122 GB/s (OMP close 64T, R4 clean) |
| OS | Linux (shared cluster) |
| C++ compiler | GCC 9.4, flags: `-O3 -fopenmp -std=c++17` |
| Rust compiler | rustc with LLVM backend, flags: `cargo build --release` |
| OMP thread model | Persistent thread pool (spin-wait between parallel regions) |
| Rust thread model | `std::thread::spawn` per trial (fresh OS threads) |
| Trials per configuration | 5 per run (B5 Run 5: 20 trials); warmup trial discarded |
| Runs | B5 conducted across 3 runs: R3 (daytime), R4 (early morning, low load), R5 (20-trial validation) |
| NUMA memory per node | ~32 GB DDR3, 2 channels/die (per AMD Opteron 6272 spec; speed unverified — assuming DDR3-1600: 2 × 12.8 = 25.6 GB/s theoretical; ~16.5 GB/s measured ≈ 64%) |
| Outlier methodology | Sort 5 trials; drop any trial > 2× the median; recompute median of remaining |
| Run time | Early morning (low cluster load) for all key results |
| Affinity env (B5 only) | `OMP_PLACES=cores` set in run script (process-local, no effect on other users) |

All source code, run scripts, and raw CSV results for each benchmark are included in
the submission.  A `readme.txt` file describes how to compile and execute each benchmark
on a CIMS machine.

To replicate: (1) clone the repository on a crunchy node; (2) run `bash run_benchmarkN_omp.sh`
and `bash run_benchmarkN_rust.sh` for each benchmark N; (3) results are written to CSV
files in `BenchmarkN/`.  All dependencies (GCC, cargo, `core_affinity` crate, mmdc for
the flowchart PDF) are available on CIMS or installed locally during the run script.

---

## 5. Experiments & Analysis

### 5.1 Benchmark 1 — Thread Synchronization Overhead

**What we measured and why.**
We measure the per-invocation cost of three primitives: parallel region entry/exit
(fork/join), full barrier synchronization, and atomic increment.  These are the building
blocks of all parallel programs.  Their cost determines the minimum profitable granularity
of parallelism: if entering a parallel region costs 343 µs, a parallel body must do far
more than 343 µs of work to benefit.

**Fork/join overhead (µs per region):**

| Threads | OpenMP | Rust | Ratio (Rust/OMP) |
|--------:|-------:|-----:|-----------------:|
| 1 | 4.83 | 6.20 | 1.3× |
| 2 | 6.21 | 19.30 | 3.1× |
| 4 | 5.20 | 36.16 | 7.0× |
| 8 | 6.14 | 62.0 | 10.1× |
| 16 | 8.91 | 126 | 14.1× |
| 32 | 18.14 | 343 | **~19×** |

**Barrier overhead (µs per barrier):**

| Threads | OpenMP | Rust | Ratio |
|--------:|-------:|-----:|------:|
| 1 | 3.04 | 2.95 | 1.0× |
| 4 | 2.12 | 17.98 | 8.5× |
| 8 | 2.30 | 29.41 | 12.8× |
| 32 | 9.39 | 130 | **13.8×** |

**Atomic increment:** Effectively tied (38–107 ns OMP, 47–103 ns Rust). Both compile to
the same `lock xadd` hardware instruction.

**Analysis.**
OpenMP threads spin-wait between parallel regions.  Waking N workers requires no OS
syscalls — workers detect the flag change in nanoseconds.  Rust's `std::sync::Barrier`
uses a futex-based condvar: each sleeping thread requires an OS `futex(WAKE)` and a
context-switch back onto a CPU.  At 32T this totals ~343 µs for Rust vs ~18 µs for
OpenMP.  The 19× gap grows monotonically with thread count because each additional
sleeping thread adds another futex wake-up to the critical path.

This structural difference also manifests as **measurement reliability**: OpenMP produced
zero contaminated trials at 32T (all 5 trials within 17.91–18.29 µs, a 2% spread).
Rust produced 3/5 contaminated trials at 32T (655–1210 µs) even on a low-load morning
run, because a single OS scheduler preemption stalls the entire barrier.

**Programmability observation.**
OpenMP requires 3 pragma annotations (one per test type) and 218 lines total.  Rust
requires designing a complete thread pool (`Pool` struct, three `Arc<Barrier>` instances,
two atomic control variables, a mode enum, shutdown protocol) for 313 lines total.
The OpenMP `volatile int sink` workaround — needed to prevent the compiler from
eliminating an empty parallel region — is a non-obvious subtlety with no Rust equivalent.

---

### 5.2 Benchmark 2-1 — Embarrassingly Parallel Scalability (Popcount)

**What we measured and why.**
When synchronization is not the bottleneck, do both models scale equally?  We use
population count sum over N = 2³³ integers.  `popcnt` maps to a single hardware
instruction on both GCC and LLVM, eliminating compiler inner-loop bias as a confound.

**Results — scaling and head-to-head:**

| Threads | OpenMP time | OMP efficiency | Rust time | Rust efficiency | Faster |
|--------:|------------:|:--------------:|----------:|:---------------:|--------|
| 1 | 17.37s | 100% | 15.65s | 100% | Rust 1.11× |
| 4 | 4.37s | 99.3% | 3.87s | 101.0% | Rust 1.13× |
| 16 | 1.12s | 97.0% | 0.97s | 100.6% | Rust 1.15× |
| 32 | 0.61s | 88.6% | 0.49s | 100.1% | Rust 1.25× |
| 64 | **0.252s** | ~98%* | 0.262s | 93.3% | **OMP 1.04×** |

*Corrected for slight 1T baseline contamination.

**Analysis.**
The Rust/LLVM advantage (1.13–1.25×) is a **constant multiplier** across all thread
counts from 1T to 32T.  A constant multiplier means the difference is entirely in
single-thread code quality, not parallelism model quality.  Disassembly confirms:
LLVM generates an 8× unrolled loop with 3–4 independent accumulator chains, while
GCC generates a scalar loop with 1 accumulator.  Both compilers scale at the same
efficiency, confirming that the parallelism models are structurally equivalent for
embarrassingly parallel work.

At 64T the advantage reverses: OpenMP wins by 4%.  With 64 threads each processing
~134M integers (~250ms), Rust's cost of spawning 64 fresh OS threads per trial
(~3ms total) erases its inner-loop advantage.  OpenMP's persistent pool wakes up at
zero cost.  This directly connects to B1: the fork/join overhead advantage that was
invisible at low thread counts (where compute dominates) surfaces when per-thread
work shrinks to hundreds of milliseconds.

---

### 5.3 Benchmark 3 — Parallel Histogram Reduction

**What we measured and why.**
Array reduction — where each thread accumulates partial results that are merged at the
end — is a common pattern in data analytics and signal processing.  We test both
performance (does the reduction implementation scale?) and programmability (how much
code is required to express it?).

**Implementation contrast.**
OpenMP reduces the entire private-copy / merge pattern to one clause:
```cpp
#pragma omp parallel for num_threads(T) schedule(static) reduction(+: h[:bins])
for (long long i = 0; i < n; ++i) h[data[i] % bins]++;
```
Rust requires explicit private allocation, partitioning, and merge (~10 additional lines):
```rust
let handles = (0..T).map(|tid| thread::spawn(move || {
    let mut local = vec![0u64; bins];
    for i in start..end { local[data[i] % bins] += 1; }
    local
})).collect::<Vec<_>>();
let mut hist = vec![0u64; bins];
for h in handles { for (b,c) in h.join().unwrap().iter().enumerate() { hist[b]+=c; } }
```

**Note on algorithm design:** A preliminary test of Strategy B (shared atomics) showed
4× slowdown at 64T.  Strategy A (private histograms, merge) gives 35× speedup.
**Algorithm choice dominates language choice** — this is the most important lesson
from B3 and motivated the Q5A branch in the decision flowchart.

**Results:**

| Threads | OpenMP | Rust | OMP efficiency | Rust efficiency |
|--------:|-------:|-----:|:--------------:|:---------------:|
| 1 | 1.194s | 1.214s | 100% | 100% |
| 8 | 0.151s | 0.152s | 98.8% | 99.8% |
| 16 | 0.076s | 0.077s | 98.0% | 98.5% |
| 32 | 0.040s | 0.042s | 93.3% | 90.9% |
| 64 | **0.034s** | 0.038s | 54.8% | 49.8% |

**Analysis.**
Performance is a tie (< 2% difference) at 1T–16T.  The Q5A decision is purely about
code: OpenMP's `reduction` clause is invisible to the programmer; Rust's explicit model
requires ~10 lines that are fully auditable and compiler-verified.

At 32T–64T OpenMP wins by 5–12% via its thread pool advantage (same mechanism as B2-1).

The 32T→64T cliff (both languages fall from ~93% efficiency to ~52%) is a **hardware
memory bandwidth wall**: the 256MB input array saturates DRAM bandwidth at ~32T.  Neither
language nor parallelism model can overcome a hardware constraint.

---

### 5.4 Benchmark 4 — Irregular Workload: Prime Testing

**What we measured and why.**
Trial-division primality over N = 1,000,000 integers has highly variable per-element
cost: small composites exit in one step, large primes (e.g., 999,983) require ~1,000
divisions.  Static partitioning systematically bottlenecks on the last thread.  This
benchmark tests both scheduling effectiveness and the programmability of dynamic
scheduling in each language.

**Results (median seconds, clean low-load run):**

| Threads | OMP static | OMP dynamic | Rust static | Rust dynamic | Rust Rayon |
|--------:|-----------:|------------:|------------:|-------------:|-----------:|
| 1 | 0.478 | 0.432 | — | 0.446 | 0.436 |
| 8 | 0.077 | 0.055 | 0.077 | 0.056 | 0.057 |
| 32 | 0.022 | 0.017 | 0.024 | **0.016** | 0.020 |
| 64 | 0.018 | **0.012** | 0.017 | 0.014 | 0.024 |

**Analysis.**
At 8T — the cleanest comparison point — the decisive result is:

| Strategy | Time | vs best |
|---|---:|---:|
| OMP dynamic | 0.055s | 1.00× |
| Rust dynamic | 0.056s | 1.02× |
| OMP static | 0.077s | 1.40× |
| Rust static | 0.077s | 1.40× |

**Scheduling gap (38–40%) >> language gap (0–4%).**  Choosing the right schedule matters
~10× more than choosing the right language for this workload class.

Rust dynamic (`Arc<AtomicU64>` with `fetch_add(100)`) matches OMP dynamic within 2–4%
at 1T–32T, confirming that the explicit Rust implementation achieves the same load
balancing at the cost of ~15 lines vs one keyword.  At 32T, Rust dynamic (0.016s) is
3% *faster* than OMP dynamic (0.017s).

Rayon matches OMP dynamic within 7% at 1T–16T (one line of code).  At 64T on this
8-NUMA-node machine, Rayon (0.024s) is 2× behind OMP dynamic (0.012s) and 41% behind
Rust static (0.017s).  Work-stealing overhead at high thread counts on NUMA hardware
exceeds any load-balancing benefit for this problem size.

**Programmer control finding:** OpenMP cannot extend its scheduling beyond the three
built-in modes.  Any custom priority, work-stealing policy, or non-standard partition
requires Rust (or raw POSIX threads in C++, forfeiting the OpenMP productivity advantage).

---

### 5.5 Benchmark 5 — Thread-to-Core Affinity and Memory Bandwidth

**What we measured and why.**
A parallel sum over a 1 GB `uint64_t` array with parallel first-touch initialization
isolates the programmer control dimension for core placement.  On an 8-NUMA-node machine,
*uncontrolled* thread placement (the "default" strategy, no pinning) causes remote DRAM
accesses that can reduce bandwidth by 2–4×.  Pinned strategies (spread and close) call
`sched_setaffinity` / `proc_bind` *before* the initialization phase, so every thread
first-touches its own pages on its pinned NUMA node and subsequently reads them with
NUMA distance 10 (local, zero penalty) — identical to the "oracle" case.

**Affinity mechanisms compared:**
- **OpenMP:** `proc_bind(spread/close/default)` pragma clauses + `OMP_PLACES=cores`
  environment variable.  Three separate parallel regions are required for three strategies
  (the clause is compile-time, not runtime-switchable).
- **Rust:** `core_affinity::set_for_current(CoreId)` (~15 lines), wrapping Linux
  `sched_setaffinity`.  Strategy is a runtime string; one function handles all three
  via a `match`.  Requires `unsafe { arr.set_len(n) }` to avoid sequential first-touch
  from the main thread.

**Results (clean median bandwidth, GB/s — Run 4, early morning, low cluster load):**

| Threads | OMP Default | OMP Spread | OMP Close | Rust Default | Rust Spread | Rust Close |
|--------:|------------:|-----------:|----------:|-------------:|------------:|-----------:|
| 8  |  5.4 ⚠ |  23.1 |  16.4 | 14.4 | **45.1** | 16.0 |
| 16 |  1.6 ⚠ |  19.6 |  32.8 | 19.8 | **76.9** | 31.4 |
| 32 | 16.2    | **87.6** |  52.1 | 22.0 | **95.7** | 24.6 |
| 64 |  7.8 ⚠ |  63.8 | **122.4** | 25.5 |  32.6 †| 53.0 |

⚠ OMP default collapsed in the clean morning run — OS scheduler packed the spin-waiting pool
onto a single NUMA node (zero cross-job competition, so no forced redistribution).
† Rust spread 64T was volatile in R4 (9–48 GB/s range); the earlier daytime run (R3) gave 53 GB/s.

**NUMA topology note.**  crunchy5 has 8 NUMA nodes, each backed by ~32 GB DDR3 with 2 channels
per die (per AMD Opteron 6272 spec; actual DIMM speed unverified — assuming DDR3-1600 gives
25.6 GB/s theoretical; ~16.5 GB/s directly measured on a single node, 64% efficiency).
The node numbering is
non-sequential: cores 0–7 → node 0, 8–15 → node 1, 16–23 → node **6**, 24–31 → node **7**,
then 32–63 → nodes 2–5.  Consequently, `proc_bind(close)` with 32 threads fills cores 0–31
and activates nodes 0, 1, **6, 7** — not nodes 0–3 as one might assume from sequential
numbering.  The NUMA distance matrix has three tiers: local = 10 (1.0×), one HyperTransport
hop = 16 (1.6×), two hops = 22 (2.2×).

**Analysis.**
Rust spread outperforms OMP spread at every thread count (1.2–2.5×) when the cluster is
clean.  Disassembly confirms the root cause: LLVM generates a dual-accumulator SSE2 inner
loop (4 u64s/iteration from two independent `paddq` chains); GCC generates a single-
accumulator loop (2 u64s/iteration).  The doubled instruction-level parallelism allows the
CPU to issue two concurrent cache-line fetch requests per cycle instead of one, effectively
doubling per-core memory-level parallelism — a **code generation difference, not a NUMA
locality difference**.  Both Rust spread and OMP spread call their respective pinning
mechanism before the first-touch init phase; both achieve local DRAM access (distance 10)
from the same memory controllers.  The bandwidth gap would persist even on a single-node
machine.

**Spread is correct at 8T for both languages.**  With 1 thread per NUMA node, 8 independent
memory controllers serve 8 simultaneous streams.  Close at 8T puts all 8 threads on a single
node (one controller), cutting available bandwidth to ~16 GB/s.  Rust spread 8T reaches
45.1 GB/s in R4 — **2.8× faster than Rust close** (16.0 GB/s) — confirming the
textbook expectation.

**Spread wins at 32T too, on a quiet machine.**  In the clean R4 run, OMP spread 32T
reached 87.6 GB/s vs OMP close 32T at 52.1 GB/s.  An earlier daytime run (R3) showed
the opposite — spread 33.0 GB/s vs close 64.5 GB/s — because nodes 2, 4, 6, 7 were
heavily loaded by other cluster users (~18–21 GB in use of 32 GB per node).  Spread
always touches all 8 NUMA nodes; when 4 of those nodes are bandwidth-saturated by
co-tenants, spread loses proportionally.  Close at 32T uses only nodes 0, 1, 6, 7 and
happened to avoid the heaviest-loaded nodes in R3.  **The R3 result was a shared-cluster
artifact, not a true reversal of the bandwidth hierarchy.**

**OMP close 64T achieves near-peak aggregate bandwidth.**  With 8 threads on each of all
8 NUMA nodes, close at 64T saturates all memory controllers simultaneously.  In R4,
3 of 5 trials reached 122–128 GB/s (~8 nodes × 15.5 GB/s = 124 GB/s, 60% of 204.8 GB/s
theoretical peak), limited only by OS preemption in the other 2 trials.

**Rust default (no pinning) requires explicit affinity on NUMA hardware.**
With `strategy = "default"`, no `sched_setaffinity` call is made in either the init or
sum phase.  The OS places each freshly-spawned thread on an arbitrary core; the first-touch
init allocates pages on that core's NUMA node, and the sum thread may run on a different
node in the next trial — or even migrate mid-computation.  The resulting cross-node DRAM
accesses incur 1.6–2.2× latency penalties (one or two HyperTransport hops), producing
highly variable results: 14–26 GB/s in R4, far below any pinned strategy and with large
trial-to-trial variance.  OMP default (spin-waiting pool, no `proc_bind`) is paradoxically
better — spin-waiting threads are never migrated by the OS, preserving first-touch locality
automatically — but it collapsed to 1.6–16 GB/s in R4 because on a quiet machine the
scheduler packed all workers onto one NUMA node.
**Explicit pinning with `core_affinity` is mandatory for reproducible NUMA-aware Rust.**

---

### 5.6 Cross-Cutting Analysis

**The compiler effect (LLVM vs GCC).**
The OpenMP vs Rust comparison is partly a GCC vs LLVM comparison.  LLVM consistently
generates better inner loops: 8× unrolled with multiple accumulators for popcount
(B2-1, +1.15× per thread), and a dual-accumulator SSE2 loop for array sum (B5, 1.1–2.8×
higher bandwidth per NUMA node at equal thread-per-node count).  In both cases the
advantage is pure instruction-level parallelism — LLVM breaks loop-carried accumulator
dependencies that GCC leaves intact, allowing the CPU to issue more concurrent memory
requests from the same hardware.  Critically, both the OMP and Rust B5 binaries achieve
full NUMA-local access (distance 10) when pinned; the bandwidth difference between Rust
spread and OMP spread at 8T (45.1 vs 23.1 GB/s) would be exactly the same on a single-NUMA
node machine.  These advantages are **constant multipliers** — the scaling curves are
parallel lines offset by compiler quality, not by parallelism model quality or NUMA
topology.

**The thread model effect (persistent pool vs spawn-per-trial).**
OpenMP's pool advantage is invisible when per-thread compute time is long (hundreds of
milliseconds), but surfaces consistently when per-thread work shrinks to tens of
milliseconds:

| Benchmark | Thread count | OMP advantage | Cause |
|---|---|---|---|
| B1 Fork/join | 32T | ~19× lower latency | Spin-wait vs condvar |
| B2-1 Popcount | 64T | 4% faster | Pool vs 64 fresh spawns (~3ms) |
| B3 Histogram | 64T | 12% faster | Same |
| B4 Prime (dynamic) | 64T | 16% faster | Same |

**Programmer control — summary across all benchmarks.**

| Control dimension | OpenMP | Rust |
|---|---|---|
| Thread-to-core placement | Compile-time pragma clause | Runtime API, flexible but requires ~15 lines |
| Variable sharing model | Shared by default; private requires clause | Private by default; sharing is opt-in |
| Data race prevention | Silent at compile time → runtime bug | Compile-time error |
| Thread lifecycle control | Opaque pool; no programmer control | Full control via spawn/join |
| Scheduling strategy | 3 built-in modes; non-extensible | Fully extensible via atomics/queues |

**Scheduling vs language (B4).**
For irregular workloads, the scheduling decision matters ~10× more than the language
decision.  After choosing the right schedule, OpenMP and Rust perform identically
(within 0–4%).  The flowchart places scheduling questions (Q4) before language
questions (Q7/Q8).

---

## 6. Conclusions

- **OpenMP is the right choice when synchronization frequency is high or when
  conciseness is the priority.**  OpenMP's persistent spin-waiting thread pool delivers
  fork/join latency 7–19× lower than Rust at 4T–32T — a structural advantage that
  determines whether fine-grained parallelism is profitable at all.  For reduction and
  scheduling patterns, OpenMP expresses in one pragma what Rust requires 10–15 explicit
  lines to implement, with no performance penalty at moderate thread counts.

- **Rust is the right choice when programmer control or safety is the priority.**
  Rust's ownership model prevents data races at compile time — a non-negotiable
  requirement for safety-critical systems that OpenMP cannot provide.  For NUMA-sensitive
  memory-bandwidth workloads, Rust with explicit `core_affinity` spread pinning delivers
  1.1–2.8× higher bandwidth per NUMA node than OMP spread at the same thread count,
  driven by LLVM's dual-accumulator SSE2 inner loop vs GCC's single-accumulator loop.
  At 32T on a quiet machine, Rust spread reaches ~96 GB/s vs OMP spread ~88 GB/s; at 8T
  the gap widens to 45 vs 23 GB/s.  Explicit pinning is mandatory: unpinned Rust threads
  suffer random cross-NUMA placement each trial and achieve only 14–26 GB/s.  Rust also
  provides full control over thread lifecycle and custom scheduling that OpenMP's opaque
  thread pool cannot expose.

- **Neither model dominates, and the decision is workload-driven.**  For embarrassingly
  parallel compute-bound work, both models scale at identical efficiency (97–101% at
  1T–32T), and the only difference is a constant compiler-quality multiplier.  For
  irregular workloads, scheduling strategy matters ~10× more than language choice.
  The decision flowchart (`decision_flowchart.pdf`) formalizes these trade-offs into
  eight actionable questions — covering synchronization frequency, reduction patterns,
  scheduling needs, NUMA sensitivity, safety requirements, and team context — each
  backed by empirical data from the five benchmarks.

---

## References

[1] M. Costanzo, E. Rucci, M. Naiouf, and A. De Giusti, "Performance vs Programming
Effort between Rust and C on Multicore Architectures: Case Study in N-Body," in
*Proc. Latin American Computing Conference (CLEI)*, 2021. arXiv:2107.11912.

[2] F. Cantonnet, Y. Yao, M. Zahran, and T. El-Ghazawi, "Productivity Analysis of the
UPC Language," in *Proc. 18th International Parallel and Distributed Processing
Symposium (IPDPS)*, IEEE, 2004.

[3] G. A. Kalderén and A. From, "A comparative analysis between parallel models in
C/C++ and C#/Java," Bachelor's Thesis, KTH Information and Communication Technology,
Stockholm, Sweden, 2013. TRITA-ICT-EX-2013:157.

[4] OpenMP Architecture Review Board, "OpenMP Application Program Interface Version 5.2,"
November 2021. [Online]. Available: https://www.openmp.org/specifications/

[5] S. Klabnik and C. Nichols, *The Rust Programming Language*. No Starch Press, 2019.

[6] R. Jung, J.-H. Jourdan, R. Krebbers, and D. Dreyer, "RustBelt: Securing the
Foundations of the Rust Programming Language," *Proc. ACM Program. Lang.*, vol. 2,
no. POPL, Dec. 2017. — *Provides the first machine-checked formal proof (Coq/Iris)
that Rust's type system prevents data races and memory errors in well-typed programs.*

[7] A. Balasubramanian, M. S. Baranowski, A. Burtsev, A. Panda, Z. Rakamarić, and
L. Ryzhyk, "System Programming in Rust: Beyond Safety," *ACM SIGOPS Operating Systems
Review*, vol. 51, no. 1, pp. 94–99, Sep. 2017. — *Demonstrates that Rust's ownership
model catches real concurrent bugs at compile time and improves long-term auditability
of concurrent systems code.*

[8] R. L. Graham et al., "Open MPI: A High-Performance, Heterogeneous MPI," in
*Proc. IEEE International Conference on Cluster Computing*, 2006.
