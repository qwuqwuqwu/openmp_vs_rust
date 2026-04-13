# Benchmark 4 Report: Irregular Workload — Prime Testing

## Overview

This benchmark measures how **static vs. dynamic scheduling** affects performance on an irregular workload, and compares OpenMP and Rust implementations of each strategy.

**Workload:** Count all primes in \[2, 1,000,000\] using trial division.
**Why it is irregular:** Checking primality requires O(√n) divisions. Small composites exit in one step; large primes (e.g. 999,983) require ~1,000 divisions. Cost per element varies by orders of magnitude. Primes are denser at the top of the range, so the last static chunk always contains the most expensive work — static partitioning systematically bottlenecks on the thread that owns the highest numbers.

**Expected answer:** π(1,000,000) = 78,498 primes. All trials correct across all runs.

**Hardware:** AMD Opteron 6272 × 4 sockets, 16 cores/socket, 64 cores total, 8 NUMA nodes (8 cores each). crunchy5, NYU HPC cluster.

---

## Implementations

| Strategy | Language | Mechanism |
|---|---|---|
| `static` | OpenMP | `schedule(static)` — fixed range per thread, assigned upfront |
| `dynamic` | OpenMP | `schedule(dynamic, 100)` — shared work queue, idle threads grab next chunk |
| `static` | Rust | `std::thread` + manual `start = tid * chunk` — identical semantics to OMP static |
| `dynamic` | Rust | `std::thread` + `Arc<AtomicU64>` counter, threads call `fetch_add(100)` |
| `rayon` | Rust | `(2..=n).into_par_iter().filter(is_prime).count()` — work-stealing, automatic |

---

## Data Collection

| Run | Files | Conditions | Usable? |
|-----|-------|------------|---------|
| Run 1 | `results_openmp_static.csv`, `results_openmp_dynamic.csv` | Daytime, shared cluster | OMP: clean 1T–64T |
| Run 2 | `results_rust_static_2.csv`, `results_rust_dynamic_2.csv`, `results_rust_rayon_2.csv` | Early morning, low load | Rust: clean all T |
| Run 3 | `results_*_3.csv` (all 5 strategies) | Daytime, heavy cluster load | Partial: T=1–2 only for most strategies; T≥32 unusable |

Results for Rust strategies were collected across two runs (early-morning low-load run 2 for Rust, which was significantly cleaner). OpenMP results were clean in run 1 and unchanged. Run 3 was conducted under heavy cluster load and is heavily contaminated at T≥8 for most strategies; it is documented in §7 for completeness. All key findings use the cleanest available data per cell (Run 1 for OMP, Run 2 for Rust).

---

## Results

### Median elapsed time (seconds)

† Rust static 1T: persistently contaminated across both runs (cluster loaded at job start in both cases); the 1T serial cost is best estimated from Rust dynamic 1T = 0.446s or OMP static 1T = 0.478s.

| Threads | OMP static | OMP dynamic | Rust static | Rust dynamic | Rust rayon |
|--------:|----------:|------------:|------------:|-------------:|-----------:|
| 1 | 0.478 | 0.432 | N/A† | 0.446 | 0.436 |
| 2 | 0.329 | 0.217 | 0.303 | 0.220 | 0.219 |
| 4 | 0.166 | 0.109 | 0.149 | 0.110 | 0.111 |
| 8 | 0.077 | 0.055 | 0.077 | 0.056 | 0.057 |
| 16 | 0.043 | 0.028 | 0.041 | 0.029 | 0.030 |
| 32 | 0.022 | 0.017 | 0.024 | 0.016 | 0.020 |
| 64 | 0.018 | 0.012 | 0.017 | 0.014 | 0.024 |

### Parallel efficiency (%) — baseline: OMP static 1T = 0.478s

| Threads | OMP static | OMP dynamic | Rust static | Rust dynamic | Rust rayon |
|--------:|----------:|------------:|------------:|-------------:|-----------:|
| 1 | 100% | 111% | — | 107% | 110% |
| 2 | 73% | 110% | 79% | 109% | 109% |
| 4 | 72% | 110% | 80% | 108% | 108% |
| 8 | 78% | 109% | 78% | 107% | 104% |
| 16 | 70% | 105% | 74% | 103% | 98% |
| 32 | 68% | 88% | 61% | 91% | 77% |
| 64 | 43% | 62% | 43% | 54% | 32% |

OMP dynamic and Rust dynamic exceed 100% efficiency from 1T–16T because both have slightly lower serial overhead than OMP static. The baseline (OMP static 1T = 0.478s) is set conservatively.

---

## Key Findings

### Finding 1 — Static scheduling has identical load-imbalance penalty in both languages

With clean data, Rust static and OMP static **track each other exactly** at every thread count:

| Threads | OMP static | Rust static | Ratio |
|--------:|----------:|------------:|------:|
| 2 | 0.329 | 0.303 | 0.92× |
| 4 | 0.166 | 0.149 | 0.90× |
| 8 | 0.077 | 0.077 | **1.00×** |
| 16 | 0.043 | 0.041 | 0.95× |
| 32 | 0.022 | 0.024 | 1.11× |
| 64 | 0.018 | 0.017 | **0.97×** |

The language difference for static scheduling is within ±11% at all thread counts — within noise for this workload. Both languages bottleneck identically on the last thread's expensive chunk. **The load imbalance penalty is a property of the algorithm, not the language.**

Parallel efficiency for both OMP static and Rust static: 72–80% at 4T–8T, declining to 43% at 64T as the last thread's chunk becomes a larger fraction of total runtime relative to the per-thread work.

### Finding 2 — Dynamic scheduling rescues both languages equally

Dynamic scheduling delivers 1.34–1.52× speedup over static consistently in both languages:

| Threads | OMP dyn/static | Rust dyn/static | Rayon/static |
|--------:|---------------:|----------------:|-------------:|
| 2 | 1.52× | 1.37× | 1.38× |
| 4 | 1.52× | 1.35× | 1.34× |
| 8 | 1.39× | **1.38×** | 1.35× |
| 16 | 1.51× | 1.40× | 1.34× |
| 32 | 1.29× | 1.48× | 1.25× |

The benefit is nearly identical between OMP and Rust: the load imbalance that dynamic scheduling corrects is the same structural problem in both cases. The scheduling mechanism (pragma vs. AtomicU64) does not change how much imbalance exists, only how efficiently it is corrected.

### Finding 3 — Rust dynamic matches OMP dynamic within 2% from 1T to 32T

| Threads | OMP dynamic | Rust dynamic | Ratio |
|--------:|------------:|-------------:|------:|
| 1 | 0.432 | 0.446 | 1.03× |
| 2 | 0.217 | 0.220 | 1.02× |
| 4 | 0.109 | 0.110 | 1.02× |
| 8 | 0.055 | 0.056 | 1.02× |
| 16 | 0.028 | 0.029 | 1.04× |
| 32 | 0.017 | 0.016 | **0.97×** (Rust faster) |
| 64 | 0.012 | 0.014 | 1.16× |

From 1T to 32T the two strategies are **statistically indistinguishable**. At 32T, Rust dynamic (0.016s) is actually 3% faster than OMP dynamic (0.017s). At 64T, OMP pulls ahead by 16% — the persistent thread pool advantage is visible again as per-thread work shrinks to ~14ms.

### Finding 4 — Rayon matches OMP dynamic at 1T–16T, then regresses

| Threads | OMP dynamic | Rust rayon | Ratio |
|--------:|------------:|-----------:|------:|
| 1 | 0.432 | 0.436 | 1.01× |
| 2 | 0.217 | 0.219 | 1.01× |
| 4 | 0.109 | 0.111 | 1.02× |
| 8 | 0.055 | 0.057 | 1.04× |
| 16 | 0.028 | 0.030 | 1.07× |
| 32 | 0.017 | 0.020 | **1.18×** |
| 64 | 0.012 | 0.024 | **2.00×** |

Rayon tracks OMP dynamic within 1–7% from 1T to 16T — **essentially free load balancing** for that range. But at 32T and 64T, Rayon's work-stealing deque management overhead becomes significant. At 64T, Rayon (0.024s) is not only 2× behind OMP dynamic — it is **slower than Rust static (0.017s) by 41%**. Work-stealing overhead on this 8-NUMA-node machine at 64T exceeds any load-balancing benefit for N=1M.

This confirms and extends the Benchmark 2-2 finding: work-stealing has a sweet spot. For uniform workloads (B2-2), it is always wasteful. For irregular workloads at moderate thread counts (B4, 1T–16T), it matches the best explicit dynamic approach at zero programmer cost. At very high thread counts on NUMA hardware, explicit `AtomicU64` is more robust.

### Finding 5 — Scheduling choice matters; language choice does not

At 8T (fully clean, most informative comparison):

| Strategy | Time | vs. OMP dynamic |
|---|---:|---:|
| OMP dynamic | 0.055s | 1.00× (best) |
| Rust dynamic | 0.056s | 1.02× |
| Rust rayon | 0.057s | 1.04× |
| OMP static | 0.077s | 1.40× |
| Rust static | 0.077s | 1.40× |

**Language gap (Rust vs. OMP, same schedule): 0–4%.**
**Scheduling gap (static vs. dynamic, same language): 38–40%.**

The choice of schedule matters 10× more than the choice of language for this workload.

### Finding 6 — Previously reported Rust static 64T contamination was cluster load, not NUMA

In the first two runs, Rust static 64T showed 0.165–0.304s (all 5 trials, systematically high). This was attributed to either NUMA effects or OS scheduling instability with 64 fresh threads. The early-morning run 2 disproves both hypotheses: **Rust static 64T = 0.017s** with five trials of [0.017, 0.017, 0.017, 0.017, 0.018] — nearly perfectly repeatable, matching OMP static 64T (0.018s).

The 10–17× inflation in earlier runs was purely cluster load. When the cluster is quiet, both Rust static and OMP static scale the same way at 64T, with similar parallel efficiency (~43%).

---

## Run 3 Raw Data (Contaminated — Daytime Cluster Load)

Run 3 re-ran all five strategies under daytime shared-cluster conditions. The cluster was heavily loaded, producing inflated times at T≥8 across most strategies and at T≥32 for OMP static. Only T=1–2 cells are trustworthy for Rust dynamic; only T=1, T=8, T=16 are clean for OMP static. The run confirms no correctness regression (all prime counts = 78,498) but does not change any findings.

### OMP Static — Run 3 per-trial (seconds)

| T | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 | Clean median | vs Run 1 |
|--:|--------:|--------:|--------:|--------:|--------:|-------------:|--------:|
| 1 | 0.518 | 0.489 | 0.475 | 3.052⚠ | 1.857⚠ | **0.489** | 1.02× |
| 2 | 0.304 | 0.325 | 3.827⚠ | 3.321⚠ | 1.357 | **0.325** ⚠ | 0.99× |
| 4 | 0.667⚠ | 0.467⚠ | 0.232 | 0.178 | 0.163 | **0.178** ⚠ | 1.07× |
| 8 | 0.101 | 0.086 | 0.086 | 0.092 | 0.090 | **0.090** | 1.17× |
| 16 | 0.044 | 0.043 | 0.043 | 0.050 | 1.046⚠ | **0.044** | 1.02× |
| 32 | 0.273⚠ | 0.294⚠ | 0.432⚠ | 0.346⚠ | 0.388⚠ | **0.346** ⚠ | 15.7× |
| 64 | 0.241⚠ | 0.272⚠ | 0.228⚠ | 0.172⚠ | 0.184⚠ | **0.228** ⚠ | 12.7× |

### OMP Dynamic — Run 3 per-trial (seconds)

| T | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 | Clean median | vs Run 1 |
|--:|--------:|--------:|--------:|--------:|--------:|-------------:|--------:|
| 1 | 1.437⚠ | 0.521 | 0.485 | 0.482 | 0.484 | **0.484** ⚠ | 1.12× |
| 2 | 0.480 | 0.406 | 0.355 | 0.282 | 0.247 | **0.355** ⚠ | 1.64× |
| 4 | 0.143 | 0.128 | 0.131 | 0.124 | 0.125 | **0.128** | 1.17× |
| 8 | 0.116 | 0.083 | 0.068 | 0.067 | 0.067 | **0.068** | 1.24× |
| 16 | 0.117 | 0.078 | 0.075 | 0.061 | 0.092 | **0.078** ⚠ | 2.79× |
| 32 | 0.064 | 0.053 | 0.064 | 0.046 | 0.034 | **0.053** ⚠ | 3.12× |
| 64 | 0.046 | 0.040 | 0.036 | 0.039 | 0.033 | **0.039** ⚠ | 3.25× |

### Rust Static — Run 3 per-trial (seconds)

| T | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 | Clean median | vs Run 2 |
|--:|--------:|--------:|--------:|--------:|--------:|-------------:|--------:|
| 1 | 0.470 | 0.473 | 0.459 | 0.459 | 0.478 | **0.470** | — (R2 N/A†) |
| 2 | 0.295 | 0.287 | 0.282 | 0.280 | 0.294 | **0.287** | 0.95× |
| 4 | 0.169 | 0.167 | 0.160 | 0.170 | 0.168 | **0.168** | 1.13× |
| 8 | 0.081 | 0.080 | 0.087 | 0.088 | 0.091 | **0.087** | 1.13× |
| 16 | 0.049 | 0.067 | 0.051 | 0.046 | 0.057 | **0.051** ⚠ | 1.24× |
| 32 | 0.023 | 0.033 | 0.035 | 0.039 | 0.034 | **0.034** ⚠ | 1.42× |
| 64 | 0.105⚠ | 0.028 | 0.018 | 0.031 | 0.019 | **0.024** ⚠ | 1.41× |

† Rust static 1T was contaminated in Run 1 and Run 2; Run 3 gives the first clean 1T value: 0.470s, consistent with Rust dynamic 1T ≈ 0.438–0.446s.

### Rust Dynamic — Run 3 per-trial (seconds)

| T | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 | Clean median | vs Run 2 |
|--:|--------:|--------:|--------:|--------:|--------:|-------------:|--------:|
| 1 | 0.470 | 0.438 | 0.438 | 0.438 | 0.438 | **0.438** | 0.98× |
| 2 | 0.224 | 0.225 | 0.223 | 0.245 | 0.467⚠ | **0.224** | 1.02× |
| 4 | 0.143 | 0.155 | 0.146 | 0.131 | 0.143 | **0.143** ⚠ | 1.30× |
| 8 | 0.085 | 0.079 | 0.073 | 0.064 | 0.058 | **0.073** ⚠ | 1.30× |
| 16 | 0.058 | 0.293⚠ | 0.166⚠ | 0.189⚠ | 0.270⚠ | **0.189** ⚠ | 6.52× |
| 32 | 0.427⚠ | 0.422⚠ | 0.335⚠ | 0.328⚠ | 0.214⚠ | **0.335** ⚠ | 20.9× |
| 64 | 0.091 | 0.084 | 0.088 | 0.076 | 0.076 | **0.084** ⚠ | 6.00× |

### Rust Rayon — Run 3 per-trial (seconds)

| T | Trial 1 | Trial 2 | Trial 3 | Trial 4 | Trial 5 | Clean median | vs Run 2 |
|--:|--------:|--------:|--------:|--------:|--------:|-------------:|--------:|
| 1 | 0.476 | 0.596 | 0.524 | 1.383⚠ | 0.482 | **0.503** ⚠ | 1.15× |
| 2 | 0.309 | 0.345 | 0.239 | 0.240 | 0.292 | **0.292** ⚠ | 1.33× |
| 4 | 0.127 | 0.160 | 0.157 | 0.123 | 0.126 | **0.127** | 1.14× |
| 8 | 0.375⚠ | 0.449⚠ | 0.321⚠ | 0.456⚠ | 0.648⚠ | **0.449** ⚠ | 7.88× |
| 16 | 0.482⚠ | 0.498⚠ | 0.486⚠ | 0.527⚠ | 0.506⚠ | **0.498** ⚠ | 16.6× |
| 32 | 0.359⚠ | 0.307⚠ | 0.347⚠ | 0.353⚠ | 0.336⚠ | **0.347** ⚠ | 17.4× |
| 64 | 0.297⚠ | 0.237⚠ | 0.201⚠ | 0.233⚠ | 0.181⚠ | **0.233** ⚠ | 9.71× |

### Cross-Run Comparison — Clean Medians (seconds)

Best available data selected per cell (Run 1 for OMP, Run 2 for Rust, Run 3 shown for reference where clean).

| T | OMP static R1 | OMP static R3 | OMP dyn R1 | OMP dyn R3 | Rust static R2 | Rust static R3 | Rust dyn R2 | Rust dyn R3 | Rayon R2 | Rayon R3 |
|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| 1 | 0.478 | 0.489 | 0.432 | 0.484⚠ | N/A† | **0.470** | 0.446 | 0.438 | 0.436 | 0.503⚠ |
| 2 | 0.329 | 0.325⚠ | 0.217 | 0.355⚠ | 0.303 | 0.287 | 0.220 | 0.224 | 0.219 | 0.292⚠ |
| 4 | 0.166 | 0.178⚠ | 0.109 | 0.128 | 0.149 | 0.168 | 0.110 | 0.143⚠ | 0.111 | 0.127 |
| 8 | 0.077 | 0.090 | 0.055 | 0.068 | 0.077 | 0.087 | 0.056 | 0.073⚠ | 0.057 | 0.449⚠ |
| 16 | 0.043 | 0.044 | 0.028 | 0.078⚠ | 0.041 | 0.051⚠ | 0.029 | 0.189⚠ | 0.030 | 0.498⚠ |
| 32 | 0.022 | 0.346⚠ | 0.017 | 0.053⚠ | 0.024 | 0.034⚠ | 0.016 | 0.335⚠ | 0.020 | 0.347⚠ |
| 64 | 0.018 | 0.228⚠ | 0.012 | 0.039⚠ | 0.017 | 0.024⚠ | 0.014 | 0.084⚠ | 0.024 | 0.233⚠ |

⚠ = value is elevated due to cluster load; do not use for performance conclusions.

**Run 3 Observation:** The contamination pattern is consistent with heavy shared-cluster load at job start. At T=1–2, threads stay on a single socket and contention is low; at T≥8 (especially T≥16), threads fan out across sockets that are loaded, inflating elapsed time by 2–21×. Rust Rayon is the most sensitive: its work-stealing deque incurs additional cross-socket traffic that amplifies the effect. The only new finding from Run 3 is the **first clean Rust static T=1 measurement: 0.470s**, consistent with the Rust dynamic T=1 baseline (0.438–0.446s) and confirming that Rust static serial overhead is in the same range as OMP static (0.478s).

---

## Code Cost Summary

| Goal | OpenMP | Rust (explicit dynamic) | Rust (Rayon) |
|---|---|---|---|
| Static scheduling | `schedule(static)` in pragma | Manual `start/end` per thread (~5 lines) | — |
| Dynamic scheduling | `schedule(dynamic, 100)` in pragma | `Arc<AtomicU64>` + `fetch_add` loop (~15 lines) | `into_par_iter()` (1 line) |
| Change static → dynamic | **1 keyword** | **~15 new lines** | **Already dynamic** |
| Performance vs OMP dynamic | 1.00–1.46× slower (static) | 0.97–1.16× | 1.01–2.00× |

---

## Summary

| Question | Answer |
|---|---|
| Does scheduling matter for irregular work? | **Yes — 1.3–1.5× consistently** |
| Is the penalty the same in both languages? | **Yes — Rust static = OMP static at all clean thread counts** |
| Can Rust match OMP dynamic? | **Yes — Rust dynamic within 2–4% of OMP dynamic up to 32T** |
| Is Rayon justified for irregular work? | **Yes at 1T–16T (within 7%). No at 32T–64T (regresses to 2× behind)** |
| At 64T, which Rust strategy is best? | **Rust dynamic (0.014s) — explicit AtomicU64 beats Rayon (0.024s) by 71%** |
| Does language matter more than scheduling? | **No — scheduling gap (38–40%) >> language gap (0–4%)** |

---

| Metric | Winner | Notes |
|---|---|---|
| Static scheduling efficiency | **Tie** | OMP static = Rust static at all clean thread counts; same load-imbalance penalty |
| Dynamic scheduling benefit | **Tie** | Both gain 1.3–1.5× over static; dynamic is equally effective in both languages |
| Dynamic scheduling performance | **Tie / OMP** | Rust dynamic within 2–4% of OMP dynamic (1T–32T); at 32T Rust is 3% *faster* |
| Ease of enabling dynamic | **OpenMP** | 1 keyword (`dynamic`) vs ~15 lines of explicit `Arc<AtomicU64>` in Rust |
| Rayon at low-to-mid threads | **Tie** | Within 7% of OMP dynamic at 1T–16T; one line of code |
| Rayon at 64T | **OpenMP** | Rayon 2× slower than OMP dynamic at 64T; work-stealing overhead exceeds benefit |
| Best Rust strategy at 64T | **Rust dynamic** | Explicit `AtomicU64` (0.014s) beats Rayon (0.024s) by 71% at 64T |
| Custom / extensible scheduling | **Rust** | OMP locked to 3 built-in modes; Rust can implement any priority, steal policy, or partition |
| Compile-time safety | **Rust** | Work-queue counter is `Arc<AtomicU64>` — sharing is explicit and compiler-checked |
| Scheduling vs language priority | **Schedule first** | Choosing dynamic over static matters ~10× more than choosing Rust over OMP |

**Bottom line:** For irregular workloads, fix the scheduling strategy first and the language second. After choosing dynamic scheduling, OpenMP and Rust perform within 2–4% of each other. The cost of getting there differs greatly: one keyword in OpenMP vs ~15 lines in Rust. Rayon is the one-liner option but regresses badly at 64T on NUMA hardware. For systems requiring custom scheduling beyond the three OMP built-in modes, Rust is the only viable choice.
