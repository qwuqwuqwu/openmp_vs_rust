# Decision Flowchart: C++/OpenMP vs Rust for Multicore Systems

> **How to use this flowchart:**
> Walk through each question from top to bottom. Answer based on your system's requirements.
> Each terminal outcome cites which benchmark provides the supporting evidence.
> All five benchmarks are complete; all nodes are backed by empirical data from crunchy5.

---

## Flowchart

```mermaid
flowchart TD
    START(["🏁 You need to build a multicore system.\nTwo options: C++ with OpenMP  |  Rust std::thread\nStart here."])
    START --> Q1

    Q1{"Q1 — Safety requirement\n\nIs freedom from data races\nor memory safety a\nnon-negotiable requirement?\n\neg. financial systems, medical,\nsafety-critical infrastructure"}
    Q1 -- Yes --> RUST_SAFE

    RUST_SAFE(["✅ Choose Rust\n\nThe Rust compiler enforces memory\nsafety and prevents data races\nat compile time — no runtime\ncost, no sanitizer needed.\n\nOpenMP offers no such guarantee.\nRace conditions are possible and\nsilent at compile time."])

    Q1 -- No --> Q2

    Q2{"Q2 — Workload shape\n\nWhat best describes your\nprimary parallel workload?"}

    Q2 -- "Loop-parallel or data-parallel\neg. array operations, matrix math,\nsimulations with uniform work" --> Q3
    Q2 -- "Irregular or dynamic\neg. graph algorithms, tree traversal,\ntask sizes that vary at runtime" --> Q4

    Q3{"Q3 — Synchronization frequency\n\nHow often does your code\nenter and exit parallel regions?\n\neg. Is the parallel body large\n(milliseconds of work each time)\nor tiny and repeated millions\nof times per second?"}

    Q3 -- "High frequency —\nfine-grained fork/join,\nmillions of regions per second" --> OPENMP_OVERHEAD

    OPENMP_OVERHEAD(["✅ Choose OpenMP\n\nBenchmark 1 result (crunchy5, 1–32T):\nOpenMP fork/join overhead:\n  1T:  4.8 µs   Rust: 6.2 µs  → 1.3×\n  2T:  6.2 µs   Rust: 19.3 µs → 3.1×\n  4T:  5.2 µs   Rust: 36.2 µs → 7.0×\n  8T:  6.1 µs   Rust: 62.0 µs → 10.1×\n  16T: 8.9 µs   Rust: 126 µs  → 14.1×\n  32T: 17.96 µs Rust: 655 µs  → 36.47×\n\nOpenMP threads spin-wait between\nregions — wake-up is nearly free.\nRust Barrier uses condvar + OS\nsyscall — latency scales with T.\n\nAt fine-grained parallelism,\nthis gap dominates total runtime."])

    Q3 -- "Low frequency —\ncoarse-grained bodies,\nthe parallel work itself\ntakes most of the time" --> Q5

    Q4{"Q4 — Scheduling needs\n\nIs OpenMP's built-in scheduling\nsufficient for your workload?\n\nschedule static  — fixed chunks\nschedule dynamic — work queue\nschedule guided  — shrinking chunks"}

    Q4 -- "Yes — one of those\ncovers my use case" --> OPENMP_SCHED

    OPENMP_SCHED(["✅ Likely OpenMP\n\nOpenMP schedule clauses handle\ncommon load-balancing patterns\nwith a single pragma.\n\nBenchmark 4 (prime testing, N=1M):\n  Dynamic vs static speedup:\n  4T:  1.52× (OMP), 1.35× (Rust)\n  8T:  1.39× (OMP), 1.38× (Rust)\n  32T: 1.29× (OMP), 1.48× (Rust)\n  Benefit is consistent and equal\n  across both languages.\n\n  Scheduling gap: 38–40%\n  Language gap (same schedule): 0–4%\n\nRust equivalent requires\n  Arc<AtomicU64> + fetch_add (~15 lines)\nRayon is 1 line but regresses at 64T\n  (2.0× behind OMP dynamic).\n\nAt 1T–32T: Rust dynamic ≈ OMP dynamic\n  (within 2–4%). Same perf, more code."])

    Q4 -- "No — I need custom\ntask priorities, stealing,\nor non-standard partitioning" --> RUST_SCHED

    RUST_SCHED(["✅ Choose Rust\n\nRust gives full programmer control\nover thread lifecycle and work\ndistribution. You can implement\nany scheduling strategy explicitly.\n\nOpenMP exposes no API to control\nthe internal thread pool or\nimplement custom scheduling.\n\nBenchmark 4 confirms:\n  Rust dynamic (Arc<AtomicU64>)\n  matches OMP dynamic at 1T–32T:\n    ratio = 0.97–1.04×\n  At 32T Rust dynamic (0.016s)\n  is 3% FASTER than OMP (0.017s).\n\n  Rayon par_iter matches OMP\n  dynamic at 1T–16T (1.01–1.07×)\n  but at 64T Rayon (0.024s) is\n  slower than Rust static (0.017s).\n  Explicit AtomicU64: 1.16× at 64T.\n\nFor custom priorities, non-standard\npartitioning, or explicit lifecycle\ncontrol: Rust is the only option.\nOpenMP cannot expose its internals."])

    Q5{"Q5 — Reduction workloads\n\nDoes your workload involve\nheavy reduction patterns?\n\neg. summing across threads,\nbuilding a shared histogram,\naccumulating partial results"}

    Q5 -- Yes --> Q5A
    Q5 -- No --> Q5B

    Q5B{"Q5B — Memory bandwidth\n\nIs your workload memory-bandwidth-\nbound on multi-socket hardware?\n\neg. scanning GB-scale arrays,\nstreaming access with trivial\narithmetic, gather/scatter"}

    Q5B -- "Yes — bandwidth and\nNUMA locality matter" --> Q5C

    Q5B -- "No — compute-bound\nor single-socket" --> Q6

    Q5C{"Q5C — Affinity trade-off\n\nPrioritize maximum raw bandwidth\n(extra code, unsafe Rust)\nor simpler code with\nslightly lower bandwidth?"}

    Q5C -- "Maximum bandwidth" --> RUST_NUMA

    Q5C -- "Simpler code,\ngood-enough bandwidth" --> OMP_NUMA

    RUST_NUMA(["✅ Likely Rust + core_affinity\n\nBenchmark 5 (1 GB array sum, crunchy5, R4 clean):\nRust spread vs OMP spread (explicit pinning):\n  8T:  45.1 vs 23.1 GB/s → 1.95×\n  16T: 76.9 vs 19.6 GB/s → 3.9×\n  32T: 95.7 vs 87.6 GB/s → 1.09×\n  64T: variable across runs (32–53 GB/s\n       for Rust vs 44–64 GB/s for OMP)\n\nRoot cause: LLVM dual-accumulator SSE2\nvs GCC single-accumulator — doubles\nILP and memory-level parallelism per thread.\nNOT a NUMA distance difference: both Rust\nspread and OMP spread call their pinning\nmechanism before first-touch init, so both\nachieve local DRAM access (distance 10).\nThe bandwidth gap would persist on a\nsingle-socket machine.\n\nCosts: ~15 lines of explicit pinning code,\nunsafe Rust (set_len + raw pointer),\ncore_affinity crate dependency.\n\nWarning: Rust without pinning (default)\nreaches only 14–26 GB/s — far below any\npinned strategy. NUMA distance penalties\n(1.6–2.2×) apply only to the unpinned\ndefault case where the OS places fresh\nthreads on arbitrary nodes each trial."])

    OMP_NUMA(["✅ Likely OpenMP\n\nBenchmark 5 (1 GB array sum, crunchy5, R4 clean):\nOMP persistent pool gives implicit NUMA\nlocality at zero extra affinity code:\n  OMP close 16T:   32.8 GB/s (very stable)\n  OMP spread 32T:  87.6 GB/s\n  OMP close 64T:  122 GB/s peak (3/5 trials)\n\nCorrected finding: spread beats close\nat 32T on a quiet machine (87.6 vs 52.1).\nA daytime run showed the reverse because\nnodes used by spread were cluster-loaded.\nSpread is NUMA-correct but cluster-fragile.\n\nOMP default is unreliable: collapsed to\n1.6–16 GB/s (R4) when OS packed the\nspin-pool onto one NUMA node. Use\nproc_bind(spread) or proc_bind(close)\nfor reproducible results.\n\nPeak is ~9% below Rust spread at 32T,\nbut no unsafe code or extra crate needed."])

    Q5A{"Q5A — What matters more\nfor your team?\n\nImplementation speed and\nconciseness\n  vs\nExplicit control and\nlong-term auditability"}

    Q5A -- "Speed and conciseness" --> OPENMP_REDUCE

    OPENMP_REDUCE(["✅ Likely OpenMP\n\nOpenMP expresses array reduction\nwith a single clause:\n  reduction(+: h[:bins])\n\nThe runtime automatically creates\nprivate copies per thread, lets\nthreads write contention-free,\nthen merges at the end.\nThe programmer writes nothing extra.\n\nBenchmark 3 (histogram, N=2²⁶,\n256 bins, Strategy A):\n  1T–16T: tie with Rust (< 2% diff)\n  32T: OpenMP 5% faster\n  64T: OpenMP 12% faster (thread pool)\n  Both scale at 98–100% efficiency\n  from 1T to 16T.\n\nRust requires ~10 extra lines:\nexplicit private Vec per thread,\nexplicit partition, explicit merge."])

    Q5A -- "Explicit control\nand auditability" --> RUST_REDUCE

    RUST_REDUCE(["✅ Likely Rust\n\nRust forces the programmer to\nexplicitly state what is shared,\nwhat is private, and how\nresults are combined.\n\nFor histogram reduction:\n  allocate Vec<u64> per thread\n  each thread fills its own copy\n  explicit merge loop after join\n\nThe borrow checker verifies at\ncompile time that no thread\naccesses another thread's private\nhistogram. No hidden sharing.\n\nBenchmark 3 confirms: same\nperformance as OpenMP at 1T–16T.\nTrade-off is ~10 extra lines of\ncode for full auditability and\ncompile-time safety guarantees."])

    Q6{"Q6 — Scalability\n\nDoes your system need to scale\nto many cores efficiently?\n\neg. Will you run at 16, 32,\nor more threads and expect\nnear-linear speedup?"}

    Q6 -- "Yes — scalability\nis a primary concern" --> OPENMP_SCALE

    OPENMP_SCALE(["✅ Likely OpenMP\n\nBoth bottleneck types favour OpenMP:\n\n① Synchronization overhead (B1, 1–32T):\nBarrier overhead — OMP vs Rust:\n  1T:  3.0 µs vs 3.0 µs  → 1.0× (tied)\n  2T:  3.7 µs vs 9.6 µs  → 2.6×\n  4T:  2.1 µs vs 18.0 µs → 8.5×\n  8T:  2.3 µs vs 29.4 µs → 12.8×\n  16T: 4.4 µs vs 62.0 µs → 14.2×\n  32T: 9.4 µs vs ~130 µs → 13.8×\nOMP barrier plateaus at ~14×\nadvantage from 8T onward.\n\n② Load imbalance → see OPENMP_SCHED\n(dynamic scheduling delivers same\nbenefit in both languages; OMP\nexpresses it in one keyword).\n\nAlso: at 64T OpenMP is 4% faster\nthan Rust (B2-1) — persistent pool\nvs spawning 64 fresh OS threads."])

    Q6 -- "No — moderate scale,\nscalability is not\nthe dominant concern" --> Q7

    Q7{"Q7 — Time horizon\n\nIs this a long-lived codebase\nor growing team where\ncorrectness and auditability\nmatter long-term?"}

    Q7 -- "Yes — long-lived system\nor growing team" --> RUST_FINAL

    RUST_FINAL(["✅ Choose Rust\n\nFor long-lived concurrent systems,\nRust's ownership model pays off:\n\n• Data races caught at compile time\n• Explicit sharing makes code\n  reviews and audits easier\n• Refactoring is safer\n• No need for sanitizers or\n  careful documentation of\n  shared/private variables\n\nHigher upfront cost,\nstronger long-term guarantees."])

    Q7 -- "No — prototype, short-lived,\nor team already in C/C++" --> OPENMP_SIMPLE

    OPENMP_SIMPLE(["✅ Likely OpenMP\n\nFor short-lived systems or C/C++ teams:\nOpenMP adds parallelism with\nminimal ramp-up and low boilerplate.\n\nBenchmark 1 implementation cost:\n  OpenMP: 218 lines, 3 pragmas\n  Rust:   313 lines, full thread\n          pool designed from scratch\n\nFor prototypes, OpenMP's fast\nresults outweigh its lack of\ncompile-time safety guarantees.\nFor C/C++ teams with a tight\ntimeline, the adoption cost\nof Rust is not justified."])
```

---

## Evidence Map

Each decision node in the flowchart is informed by one or more benchmarks.
This table will be filled in as benchmarks are completed.

| Question | Key metric | Benchmark | Status |
|---|---|---|---|
| Q3 — Sync frequency | Fork/join overhead vs thread count (1–32T) | Benchmark 1 (re-run) | ✅ Complete |
| Q5 — Reduction workloads | Reduction LOC, runtime, correctness effort | Benchmark 3 | ✅ Complete |
| Q6 — Scalability | Barrier cost 1–32T, pool vs spawn overhead, speedup curves | Benchmark 1 + 2-1 | ✅ Complete |
| Q4 — Scheduling needs | LOC, flexibility, runtime comparison; load imbalance correction | Benchmark 4 | ✅ Complete |
| Q5B/Q5C — NUMA bandwidth | Bandwidth vs affinity strategy, 8–64T | Benchmark 5 | ✅ Complete |
| Q7 — Time horizon | LOC cost, safety guarantees, long-term maintenance trade-off | Benchmark 1 | ✅ Complete |

---

## Key Findings (Benchmark 1 — Thread Overhead)

These facts are established from the thread overhead microbenchmark (fork/join, barrier, atomic, 1–32 threads, crunchy5 early-morning low-load run):

1. **OpenMP fork/join overhead is 1.3–36× lower than Rust** from 1T to 32T, with the gap growing with thread count.
   1T: 4.8 µs vs 6.2 µs (1.3×) — 2T: 6.2 µs vs 19.3 µs (3.1×) — 4T: 5.2 µs vs 36.2 µs (7.0×) — 8T: 6.1 µs vs 62.0 µs (10.1×) — 16T: 8.9 µs vs 126 µs (14.1×) — 32T: 17.96 µs vs 655 µs (36.47×).
   OpenMP: 3.8× growth from 1T to 32T. Rust: ~55× growth.
   → Drives Q3: if sync frequency is high, OpenMP wins. The gap is decisive and grows monotonically.

2. **Barrier overhead is equal at 1 thread, then diverges sharply and plateaus.**
   1T: tied (~3.0 µs each). 2T: 2.6×. 4T: 8.5×. 8T: 12.8×. 16T: 14.2×. 32T: 13.8×.
   OpenMP barrier: 2.1–9.4 µs (1T–32T). Rust barrier: 3.0–130 µs.
   Ratio plateaus at ~14× from 8T onward — both scale, but with a stable ~14× cost multiplier.
   → Reinforces Q6: if synchronization is the bottleneck, OpenMP scales better by ~14× at 8T+.

3. **Atomic increment cost is essentially identical** (38–107 ns OMP, 47–103 ns Rust, no consistent winner at any thread count).
   → Neither language has an advantage on hardware atomics.

4. **OpenMP provides no programmer control over thread lifecycle.**
   You cannot force thread creation or destruction between regions.
   → Drives Q4: if custom scheduling or lifecycle control is needed, OpenMP cannot do it.

5. **Rust required 95 more lines of code** and a full thread pool design for an equivalent benchmark.
   → Drives Q7/Q8: for rapid development, OpenMP wins on productivity.

6. **Rust's borrow checker caught all sharing bugs at compile time.** No runtime debugging needed.
   → Drives Q1/Q8: for safety-critical or long-lived systems, Rust's upfront cost is justified.

7. **OMP's persistent spin-pool produces 0 contaminated cells across all B1 trials.** Rust's condvar-based barrier produced contaminated cells even in low-load conditions at T=16 and T=32 (2–3 out of 5 trials). When threads sleep, OS scheduler preemptions can stall the entire barrier.
   → Reinforces Q3: OpenMP's spin-waiting is a reliability advantage on shared hardware, not just a latency advantage.

---

## Key Findings (Benchmark 2 — Monte Carlo Pi)

These facts are established from the Monte Carlo Pi benchmark (Xorshift64 RNG, 1B samples, 1–64 threads):

7. **RNG choice introduced a 3× artificial performance gap.** The original comparison used mt19937_64 (C++ stdlib) vs Xorshift64 (Rust). Unifying both to Xorshift64 collapsed the gap to ~1.35×. Confounding factors in the inner loop must be eliminated before comparing parallel models.
   → Lesson: benchmark design determines what you are actually measuring. A 3× gap from RNG choice was masking the true parallelism comparison.

8. **With unified RNG, OpenMP is ~1.35× faster at all thread counts — a constant multiplier.**
   A constant ratio across all thread counts means the difference is entirely in single-thread code generation (Xorshift64's sequential dependency chain preventing SIMD), not in parallelism quality. Both models scale at the same rate.
   → Precursor to Benchmark 2-1: the FP and RNG inner loop must be replaced with a bias-free workload to isolate the parallelism model.

---

## Key Findings (Benchmark 2-1 — Embarrassingly Parallel Scalability, Popcount)

These facts are established from the popcount benchmark (N = 2³³, 1–64 threads, both implementations clean):

9. **Both OpenMP and Rust scale at near-identical efficiency for compute-bound embarrassingly parallel work.**
   Both reach 97–100% parallel efficiency from 1T to 16T. Neither parallelism model has a structural scalability advantage when synchronization is absent.
   → Q6 confirmed: if scalability is the concern, the decision must be made on other axes.

10. **Rust/LLVM is ~1.13–1.15× faster than C++/GCC from 1T to 16T.**
    The gap is a **constant multiplier** — it does not change as thread count scales. This proves the difference is in single-thread code generation (LLVM's 8× loop unrolling vs GCC's scalar loop), not in the parallelism model.
    → Reinforces Q7/Q8: LLVM produces better code for this workload, but this does not affect the parallel scaling decision.

11. **At 64 threads, OpenMP reverses the advantage and runs 4% faster than Rust.**
    With 64 threads each doing only ~250ms of compute, Rust's cost of spawning 64 fresh OS threads per trial (~3ms total) erases its inner-loop speed advantage. OpenMP's persistent thread pool wakes up at nearly zero cost.
    → Reinforces Q3: fine-grained or high-thread-count parallelism favors OpenMP's thread pool model.

12. **Neither compiler uses AVX2 SIMD for popcount on crunchy.**
    AVX-512 VPOPCNTDQ is not available on the cluster. Both binaries use scalar `popcnt`. LLVM's advantage comes from 8× loop unrolling and multiple accumulators, not SIMD width.

---

## Key Findings (Benchmark 2-2 — Rayon vs std::thread)

These facts are established from repeating the popcount benchmark with Rayon (N = 2³³, 1–64 threads):

13. **Switching from bare `std::thread` to Rayon does not fix the 64T reversal — it makes it larger.**
    OpenMP leads Rayon by 7.5% at 64T vs only 4% over std::thread. Rayon's work-stealing bookkeeping across 64 workers compounds the thread management overhead.
    → The 64T reversal is not caused by thread spawn cost alone; it is a structural property of OpenMP's persistent pool vs any fresh-thread or work-stealing model.

14. **For uniform workloads, bare `std::thread` with static partitioning beats Rayon.**
    std::thread is 3–5% faster than Rayon at every thread count from 1T to 32T. LLVM generates an 8× unrolled loop inside the bare thread closure vs only 4× inside Rayon's work-stealing closure. Work-stealing adds scheduler overhead that buys nothing when every element costs the same.
    → Rayon's advantage is for **irregular** workloads where load imbalance exists. For perfectly uniform work, static partitioning is always better.

15. **The performance ranking for uniform workloads is: std::thread > Rayon > OpenMP (1T–32T); OpenMP > std::thread > Rayon (64T).**
    All three scale at equivalent efficiency. The differences are in single-thread code quality and thread management overhead, not in parallelism model quality.

---

## Key Findings (Benchmark 3 — Parallel Histogram, Reduction Workloads)

These facts are established from the histogram benchmark (N = 2²⁶, 256 bins, Strategy A, 1–64 threads):

16. **Algorithm design matters more than language choice.**
    Strategy B (shared atomics) made performance 4× *worse* at 64 threads. Strategy A (private histograms + merge) gives 35× speedup at 64T. Choosing the right reduction pattern is the first decision — language comes second.

17. **Performance is a tie at 1T–16T.** OpenMP and Rust differ by less than 2% at every thread count from 1T to 16T. Both scale at 98–100% parallel efficiency. The Q5A decision is entirely about code, not runtime.
    → Q5A confirmed: the choice between OpenMP and Rust for reduction workloads is a programmability decision, not a performance decision.

18. **OpenMP's `reduction` clause expresses the entire pattern in one line.**
    The programmer writes a serial-looking loop; the runtime handles private allocation, contention-free writes, and merge invisibly. Rust requires ~10 explicit lines for the same behavior.
    → Drives `OPENMP_REDUCE`: for teams that prioritize conciseness and development speed, OpenMP wins on this axis.

19. **Rust's explicit model gives compile-time auditability.**
    Every decision is visible: what each thread owns, what it writes, and when the merge happens. The borrow checker verifies at compile time that no thread accesses another's private data.
    → Drives `RUST_REDUCE`: for long-lived systems where correctness and code review clarity matter, Rust's verbosity is a feature.

20. **The 32T→64T cliff is a memory bandwidth wall, not a parallelism problem.**
    Both languages collapse from ~94% efficiency at 32T to ~50% at 64T. The 256MB input array saturates memory bandwidth at ~32 threads. This is a hardware constraint that neither language can overcome.

---

## Key Findings (Benchmark 4 — Irregular Workload, Prime Testing)

These facts are established from the prime testing benchmark (N = 1,000,000, 5 strategies, 1–64 threads):

21. **Static scheduling has identical load-imbalance penalty in both languages.**
    With clean data (early-morning low-load run), Rust static and OMP static track each other within ±11% at every thread count:
      8T:  OMP static = 0.077s, Rust static = 0.077s (tied exactly)
      64T: OMP static = 0.018s, Rust static = 0.017s (Rust 6% faster)
    Both achieve 43% parallel efficiency at 64T. The load imbalance bottleneck is a property of the algorithm, not the language.
    → Confirms Q6A: for irregular workloads, static scheduling is the wrong choice regardless of language.

22. **Dynamic scheduling delivers 1.3–1.5× speedup over static, identically in both languages.**
    OMP dynamic/static: 1.52× at 4T, 1.39× at 8T, 1.29× at 32T.
    Rust dynamic/static: 1.35× at 4T, 1.38× at 8T, 1.48× at 32T.
    The benefit is consistent and nearly equal between languages. Scheduling mechanism (pragma vs. AtomicU64) does not change how much imbalance is corrected — only how much code is needed.
    → Confirms Q4 (OPENMP_SCHED): OpenMP's one-keyword change delivers the same benefit as ~15 lines of explicit Rust code.

23. **Rust dynamic matches OMP dynamic within 2–4% from 1T to 32T.**
    Rust dynamic: 0.446s, 0.220s, 0.110s, 0.056s, 0.029s, 0.016s at 1T/2T/4T/8T/16T/32T.
    OMP dynamic:  0.432s, 0.217s, 0.109s, 0.055s, 0.028s, 0.017s at same counts.
    At 32T, Rust dynamic (0.016s) is 3% *faster* than OMP dynamic (0.017s).
    The explicit `Arc<AtomicU64>` counter achieves the same load-balancing effect as OpenMP's internal work queue.
    → Confirms Q4 (RUST_SCHED): Rust can match OMP dynamic, but requires ~15 lines vs. one keyword.

24. **Rayon matches OMP dynamic at 1T–16T (1.01–1.07×), then regresses sharply.**
    Rayon: 0.436s, 0.219s, 0.111s, 0.057s, 0.030s at 1T–16T — within 7% of OMP dynamic.
    At 32T: Rayon = 0.020s vs OMP dynamic 0.017s → 1.18× behind.
    At 64T: Rayon = 0.024s vs OMP dynamic 0.012s → **2.0× behind**.
    At 64T, Rayon (0.024s) is also **41% slower than Rust static (0.017s)** — work-stealing overhead on this 8-NUMA-node machine exceeds any load-balancing benefit at N=1M, 64T.
    → Nuance for Q4: Rayon is the highest-productivity option at 1T–16T (1 line, near-optimal). Explicit dynamic (AtomicU64) is better at 32T–64T on NUMA hardware.

25. **Scheduling gap (38–40%) vastly exceeds language gap (0–4%).**
    At 8T (fully clean, all 5 strategies populated):
      OMP dynamic: 0.055s (best)
      Rust dynamic: 0.056s (1.02× — language gap)
      Rust rayon:  0.057s (1.04× — language gap)
      OMP static:  0.077s (1.40× — scheduling gap)
      Rust static: 0.077s (1.40× — scheduling gap, same as OMP!)
    The scheduling gap (40%) is ~10× the language gap (2–4%). For irregular workloads: choose the schedule first, the language second.
    → Reinforces Q4 over Q7/Q8.

26. **Previously anomalous Rust static 64T results were cluster load, not NUMA.**
    Earlier runs showed Rust static 64T = 0.165–0.304s (all 5 trials, 10–17× slower than expected).
    Early-morning low-load run 2 shows Rust static 64T = **0.017s** (five trials: 0.017, 0.017, 0.017, 0.017, 0.018).
    This definitively rules out NUMA effects or structural load imbalance as the cause — both are reproducible properties that would persist even at low cluster load. The anomaly was pure cluster interference.
    OpenMP's persistent pool produced 0 contaminated cells in all runs. Rust's fresh-thread spawning is more sensitive to cluster load, but produces identical performance when the cluster is quiet.
    → Reinforces Q3: OpenMP's persistent pool is a reliability advantage on shared hardware, not just a latency advantage.

---

## Key Findings (Benchmark 5 — Thread-to-Core Affinity, Memory Bandwidth)

These facts are established from the parallel array sum benchmark (1 GB uint64_t, 3 affinity strategies, 8–64 threads, crunchy5):

27. **Explicit NUMA-aware pinning is essential for memory-bandwidth workloads on multi-socket hardware.**
    Rust default (no pinning): 14–26 GB/s (R4 clean). Rust spread (explicit sched_setaffinity): 45–96 GB/s. Same hardware, same thread count — a 2–7× gap. The gap has two causes: (a) unpinned threads land on arbitrary NUMA nodes, causing cross-node DRAM latency penalties of 1.6–2.2× per hop; (b) multiple threads may pile onto the same node, reducing total controller count. This NUMA distance penalty applies only to the *default* (unpinned) case — spread and close call sched_setaffinity before first-touch init, achieving local access (distance 10) with zero penalty.
    OpenMP's persistent thread pool avoids the worst-case (OMP spread/close: 16–122 GB/s), but OMP default collapsed to 1.6–16 GB/s in R4 when the OS packed the idle spin-pool onto a single NUMA node on a quiet machine.
    → Drives Q5B: if the workload is memory-bandwidth-bound, placement strategy is the dominant variable.

28. **Rust spread with core_affinity outperforms OMP spread at 8T–32T on a clean machine.**
    R4 clean run: 8T: 45.1 vs 23.1 GB/s (1.95×) — 16T: 76.9 vs 19.6 GB/s (3.9×) — 32T: 95.7 vs 87.6 GB/s (1.09×). At 64T, Rust spread was volatile in R4 (9–48 GB/s range); earlier daytime run: 53.3 vs 44.0 GB/s (1.21×).
    Root cause: LLVM generates a dual-accumulator SSE2 inner loop (4 u64s/iteration) vs GCC's single-accumulator loop (2 u64s/iteration), doubling instruction-level parallelism and memory-level parallelism per thread. This is a **code-generation difference, not a NUMA locality difference** — both Rust spread and OMP spread call their pinning mechanism before the first-touch init phase and both achieve NUMA-local access (distance 10) from the same memory controllers. The bandwidth gap would be identical on a single-socket machine.
    → Drives Q5C → RUST_NUMA: for maximum bandwidth at 8T–32T, Rust + explicit spread pinning is the right choice.

29. **Spread beats close on a dedicated machine; close appears to win only when spread's NUMA nodes are cluster-loaded.**
    R4 clean: OMP spread 32T = 87.6 GB/s vs OMP close 32T = 52.1 GB/s. Spread correctly exploits all 8 NUMA nodes.
    A daytime run (R3) showed the opposite — spread 33.0 GB/s vs close 64.5 GB/s — because 4 of the 8 nodes were heavily loaded by other cluster users (~21 GB in use of 32 GB). Spread always touches all nodes; one busy node costs ~12.5% of total bandwidth.
    Also: close 32T activates nodes 0, 1, **6, 7** (not 0–3): core 16 → node 6, core 24 → node 7 due to non-sequential NUMA numbering on crunchy5.
    → Practical implication: spread is the NUMA-correct strategy for bandwidth; close is more robust on shared clusters. The apparent "close wins" result is a cluster-load artifact.

30. **OMP close 64T achieves near-peak aggregate bandwidth — but is bimodal.**
    R4: OMP close 64T = 8 threads/node × 8 nodes, all memory controllers active. 3 of 5 trials reached 122–128 GB/s (~60% of 204.8 GB/s theoretical peak, or ~8 × 15.5 GB/s measured per node). 2 trials collapsed to 63 GB/s due to OS preemption of a thread during the sum pass.
    OMP default is unreliable: on a quiet machine it collapsed to 1.6–16 GB/s (R4) because idle spin-waiting threads were packed onto a single NUMA node by the scheduler. Use proc_bind(spread) or proc_bind(close) explicitly; never rely on default for bandwidth workloads.
    → Drives Q5C → OMP_NUMA: OMP close 64T is the highest-bandwidth OpenMP configuration. For fewer than 64 threads, OMP spread is correct.

31. **Rust's default thread model is NUMA-blind and produces high variance (pinned strategies are unaffected).**
    `std::thread::spawn` with no `sched_setaffinity` call places threads on arbitrary cores each trial. The first-touch init then allocates pages on those arbitrary nodes; the subsequent read may cross NUMA node boundaries, incurring a 1.6–2.2× latency penalty (1 or 2 HyperTransport hops). Because fresh threads are re-rolled each trial, the penalty is random and irreproducible. In R4, Rust default 8T ranged 14.2–14.6 GB/s (lucky stable placement); in R3, trial 1 reached 19.57 GB/s then collapsed to 7–9 GB/s for subsequent trials.
    This issue is specific to `strategy = "default"`. Rust spread and Rust close call `core_affinity::set_for_current()` as the first action in each spawned thread — before the init phase — so both achieve local DRAM access (distance 10) with zero cross-node penalty.
    → Reinforces Q5B → Q5C: any Rust implementation of a bandwidth-bound workload on NUMA hardware must include explicit core_affinity pinning. The default is not a safe baseline.

32. **Peak measured memory bandwidth on crunchy5 across all strategies and runs:**
    - Rust spread 32T (R4): ~96 GB/s
    - OMP spread 32T (R4): ~88 GB/s
    - OMP close 64T (R4, clean trials): ~122–128 GB/s
    crunchy5 is a **Dell PowerEdge R815** (confirmed via `/sys/class/dmi/id/product_name`). It has 8 NUMA nodes, each with 2 DDR3 memory channels per die (confirmed: AMD Opteron 6272 spec + Dell R815 spec: 4 channels/socket ÷ 2 dies = 2/die; also empirically supported because measured 16.5 GB/s/node exceeds any single-channel DDR3 ceiling). DIMM speed is DDR3-1600 per the CPU's max-supported spec but not directly verified (`dmidecode` requires root). Assuming DDR3-1600: 25.6 GB/s theoretical per node; measured ~16.5 GB/s per node (~64% efficiency), consistent with STREAM benchmark results.
    OMP close 64T at ~122 GB/s is the absolute peak: 8 nodes × 15.25 GB/s ≈ 122 GB/s, confirming per-node saturation not aggregate-bus saturation.

---

## How to Read the Final Outcome

The flowchart will rarely give a perfect one-sided answer. In practice:

- If multiple paths point to **OpenMP** → OpenMP is the right fit for your system.
- If multiple paths point to **Rust** → Rust is the right fit.
- If paths are mixed → read the cited benchmark numbers for your specific bottleneck and weight the decision toward whichever axis matters most for your system.

The flowchart is a structured way to surface the right trade-off questions, not a mechanical oracle.
