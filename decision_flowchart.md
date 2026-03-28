# Decision Flowchart: C++/OpenMP vs Rust for Multicore Systems

> **How to use this flowchart:**
> Walk through each question from top to bottom. Answer based on your system's requirements.
> Each terminal outcome cites which benchmark provides the supporting evidence.
> Nodes marked **[TBD]** will be updated as Benchmarks 2–4 are completed.

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

    OPENMP_OVERHEAD(["✅ Choose OpenMP\n\nBenchmark 1 result:\nOpenMP fork/join overhead is\n5–16× lower than Rust at\n2–8 threads on crunchy.\n\nOpenMP threads spin-wait between\nregions — wake-up is nearly free.\nRust Barrier uses condvar + OS\nsyscall — latency scales with\nthread count.\n\nAt fine-grained parallelism,\nthis gap dominates total runtime."])

    Q3 -- "Low frequency —\ncoarse-grained bodies,\nthe parallel work itself\ntakes most of the time" --> Q5

    Q4{"Q4 — Scheduling needs\n\nIs OpenMP's built-in scheduling\nsufficient for your workload?\n\nschedule static  — fixed chunks\nschedule dynamic — work queue\nschedule guided  — shrinking chunks"}

    Q4 -- "Yes — one of those\ncovers my use case" --> OPENMP_SCHED

    OPENMP_SCHED(["✅ Likely OpenMP\n\nOpenMP schedule clauses handle\ncommon load-balancing patterns\nwith a single pragma.\n\nRust requires a manual work queue\n(channel or mutex-protected deque)\nwhich is significantly more code.\n\n[TBD — update with Benchmark 4\nirregular workload results]"])

    Q4 -- "No — I need custom\ntask priorities, stealing,\nor non-standard partitioning" --> RUST_SCHED

    RUST_SCHED(["✅ Choose Rust\n\nRust gives full programmer control\nover thread lifecycle and work\ndistribution. You can implement\nany scheduling strategy explicitly.\n\nOpenMP exposes no API to control\nthe internal thread pool or\nimplement custom scheduling.\nThis is a programmer control\nlimitation of OpenMP.\n\n[TBD — update with Benchmark 4]"])

    Q5{"Q5 — Reduction workloads\n\nDoes your workload involve\nheavy reduction patterns?\n\neg. summing across threads,\nbuilding a shared histogram,\naccumulating partial results"}

    Q5 -- Yes --> Q5A
    Q5 -- No --> Q6

    Q5A{"Q5A — What matters more\nfor your team?\n\nImplementation speed and\nconciseness\n  vs\nExplicit control and\nlong-term auditability"}

    Q5A -- "Speed and conciseness" --> OPENMP_REDUCE

    OPENMP_REDUCE(["✅ Likely OpenMP\n\nOpenMP expresses reduction\nwith a single clause:\n  #pragma omp parallel reduction(+:sum)\n\nRust requires explicit per-thread\nlocal accumulators, a join step,\nand careful ownership handling.\n\n[TBD — update with Benchmark 3\nhistogram / dot product results]"])

    Q5A -- "Explicit control\nand auditability" --> RUST_REDUCE

    RUST_REDUCE(["✅ Likely Rust\n\nRust forces the programmer to\nexplicitly state what is shared,\nwhat is private, and how\nresults are combined.\n\nThe borrow checker verifies\ncorrectness at compile time.\nNo hidden sharing bugs.\n\n[TBD — update with Benchmark 3]"])

    Q6{"Q6 — Scalability\n\nDoes your system need to scale\nto many cores efficiently?\n\neg. Will you run at 16, 32,\nor more threads and expect\nnear-linear speedup?"}

    Q6 -- "Yes — scalability\nis a primary concern" --> Q6A

    Q6A{"Q6A — What limits\nyour scalability?\n\nSynchronization overhead\n  vs\nLoad imbalance"}

    Q6A -- "Synchronization overhead\nis the bottleneck" --> OPENMP_SCALE

    OPENMP_SCALE(["✅ Likely OpenMP\n\nBenchmark 1 shows OpenMP\nbarrier and fork/join overhead\nscales far better than Rust's\ncondvar-based primitives.\n\nAt 8 threads:\n  OpenMP barrier: ~4 µs\n  Rust barrier:   ~55 µs\n\nBenchmark 2-1 (popcount, N=2³³):\nAt 64 threads, OpenMP is 4% faster\nthan Rust despite a slower inner\nloop — OpenMP's persistent thread\npool wakes up instantly while Rust\nspawns 64 fresh OS threads each\ntrial (~3 ms overhead per run).\n\nCrossover: 32T→64T, where per-thread\nwork drops to ~250 ms and thread\nmanagement overhead becomes visible."])

    Q6A -- "Load imbalance\nis the bottleneck" --> OPENMP_SCHED

    Q6 -- "No — moderate scale,\nscalability is not\nthe dominant concern" --> Q7

    Q7{"Q7 — Team and timeline\n\nWhat is your team's situation?"}

    Q7 -- "Team is already in C/C++\nand timeline is tight" --> OPENMP_TEAM

    OPENMP_TEAM(["✅ Likely OpenMP\n\nOpenMP adds parallelism to\nexisting C/C++ code with minimal\nnew concepts. Ramp-up is low\nfor a C/C++ team.\n\nBenchmark 1 implementation:\n  OpenMP: 218 lines\n  Rust:   313 lines\n\nOpenMP required 3 pragmas.\nRust required designing a\nfull thread pool from scratch."])

    Q7 -- "Team is new to both\nor long-term investment\nis acceptable" --> Q8

    Q8{"Q8 — Time horizon\n\nHow long will this codebase live\nand how many people will\nwork on it?"}

    Q8 -- "Short-lived prototype\nor small team, speed matters" --> OPENMP_PROTO

    OPENMP_PROTO(["✅ Likely OpenMP\n\nFor short-lived or\nexperimental code, OpenMP's\nlow boilerplate and fast\nresults outweigh its\nlack of safety guarantees."])

    Q8 -- "Long-lived system\nor growing team\ncorrectness matters" --> RUST_FINAL

    RUST_FINAL(["✅ Choose Rust\n\nFor long-lived concurrent systems,\nRust's ownership model pays off:\n\n• Data races caught at compile time\n• Explicit sharing makes code\n  reviews and audits easier\n• Refactoring is safer\n• No need for sanitizers or\n  careful documentation of\n  shared/private variables\n\nHigher upfront cost,\nstronger long-term guarantees."])
```

---

## Evidence Map

Each decision node in the flowchart is informed by one or more benchmarks.
This table will be filled in as benchmarks are completed.

| Question | Key metric | Benchmark | Status |
|---|---|---|---|
| Q3 — Sync frequency | Fork/join overhead vs thread count | Benchmark 1 | ✅ Complete |
| Q5 — Reduction workloads | Reduction LOC, runtime, correctness effort | Benchmark 3 | 🔲 Pending |
| Q6A — Sync overhead at scale | Barrier cost, speedup curves | Benchmark 1 + 2-1 | ✅ Complete |
| Q6A — Load imbalance | Runtime under uneven work, scheduling | Benchmark 4 | 🔲 Pending |
| Q4 — Custom scheduling | LOC, flexibility, runtime comparison | Benchmark 4 | 🔲 Pending |
| Q6 — Scalability | Speedup and efficiency vs thread count | Benchmark 2-1 | ✅ Complete |

---

## Key Findings So Far (Benchmark 1)

These facts are established and directly feed into the flowchart:

1. **OpenMP fork/join overhead is 5–16× lower than Rust** at 2–8 threads.
   → Drives Q3: if sync frequency is high, OpenMP wins.

2. **Barrier overhead is equal at 1 thread, then diverges sharply.**
   OpenMP: ~2–4 µs. Rust: ~16–55 µs at 2–8 threads.
   → Reinforces Q6A: if synchronization is the bottleneck, OpenMP scales better.

3. **Atomic increment cost is identical** (~24–102 ns on both sides, within noise).
   → Neither language has an advantage on hardware atomics.

4. **OpenMP provides no programmer control over thread lifecycle.**
   You cannot force thread creation or destruction between regions.
   → Drives Q4: if custom scheduling or lifecycle control is needed, OpenMP cannot do it.

5. **Rust required 95 more lines of code** and a full thread pool design for an equivalent benchmark.
   → Drives Q7/Q8: for rapid development, OpenMP wins on productivity.

6. **Rust's borrow checker caught all sharing bugs at compile time.** No runtime debugging needed.
   → Drives Q1/Q8: for safety-critical or long-lived systems, Rust's upfront cost is justified.

---

## Key Findings (Benchmark 2-1 — Embarrassingly Parallel Scalability)

These facts are established from the popcount benchmark (N = 2³³, 1–64 threads, both implementations clean):

7. **Both OpenMP and Rust scale at near-identical efficiency for compute-bound embarrassingly parallel work.**
   Both reach 97–100% parallel efficiency from 1T to 16T. Neither parallelism model has a structural scalability advantage when synchronization is absent.
   → Q6 confirmed: if scalability is the concern, the decision must be made on other axes.

8. **Rust/LLVM is ~1.13–1.15× faster than C++/GCC from 1T to 16T.**
   The gap is a **constant multiplier** — it does not change as thread count scales. This proves the difference is in single-thread code generation (LLVM's 8× loop unrolling vs GCC's scalar loop), not in the parallelism model. The scaling curves are parallel lines.
   → Reinforces Q7/Q8: for pure throughput on a single machine, LLVM produces better code, but this does not affect the parallel scaling decision.

9. **At 64 threads, OpenMP reverses the advantage and runs 4% faster than Rust.**
   With 64 threads each doing only ~250ms of compute, Rust's cost of spawning 64 fresh OS threads per trial (~3ms total) erases its inner-loop speed advantage. OpenMP's persistent thread pool wakes up at nearly zero cost.
   → Reinforces Q3: fine-grained or high-thread-count parallelism favors OpenMP's thread pool model. The Q3 `OPENMP_OVERHEAD` outcome is now supported by both Benchmark 1 (microsecond fork/join) and Benchmark 2-1 (64T reversal at 250ms/thread).

10. **Neither compiler uses AVX2 SIMD for popcount on crunchy.**
    AVX-512 VPOPCNTDQ is required for hardware-vectorized popcount; the cluster does not have it. Both compilers emit scalar `popcnt`. The LLVM advantage comes from loop unrolling and multiple accumulators, not SIMD width.

---

## How to Read the Final Outcome

The flowchart will rarely give a perfect one-sided answer. In practice:

- If multiple paths point to **OpenMP** → OpenMP is the right fit for your system.
- If multiple paths point to **Rust** → Rust is the right fit.
- If paths are mixed → read the cited benchmark numbers for your specific bottleneck and weight the decision toward whichever axis matters most for your system.

The flowchart is a structured way to surface the right trade-off questions, not a mechanical oracle.
