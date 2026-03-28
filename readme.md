Comparing OpenMP and Rust on Multicore Processors
Project Plan
1. Project Goal

This project compares OpenMP and Rust for multicore programming based on:

Programmability
How easy it is to write, understand, debug, and modify parallel code.

Scalability
How performance changes as the number of threads increases.

Runtime overhead
The cost of creating threads, synchronizing threads, and managing shared state.

Programmer control
How much control the programmer has over thread placement, work distribution, shared/private data, and synchronization.

Performance
Runtime on the same machine with the same problem size.

The final goal is to produce a set of guidelines for when to use OpenMP and when to use Rust.

2. Overall Strategy

We will implement at least four benchmark programs in both OpenMP and Rust.
Each benchmark will emphasize one or more of the required comparison points.

To keep the comparison fair:

both versions will solve the same problem

both versions will use the same input size

both versions will run on the same crunchy node

both versions will be compiled with optimization enabled

all tests will be repeated multiple times

3. Languages and Runtime Models
OpenMP side

Language: C or C++

Parallel model: OpenMP shared-memory multithreading

Rust side

Language: Rust

Parallel model: Rust standard library threads

std::thread

Arc

Mutex

Barrier

atomics where needed

Why use Rust standard library threads

This keeps the comparison closer to:

OpenMP runtime model

language-level parallel programming effort

explicit control over threads and synchronization

Using Rayon would make programming easier, but it would shift the comparison toward OpenMP vs a Rust parallel library, which is less direct for this project.

4. Benchmark Set

We will use four benchmarks.

Benchmark 1: Thread Overhead Microbenchmark
Purpose

Measure runtime overhead.

What to test

thread creation

thread termination / join

barrier synchronization

mutex or atomic update overhead

OpenMP version

repeatedly enter/exit parallel regions

use #pragma omp barrier

use reduction / critical / atomic where appropriate

Rust version

repeatedly create threads with std::thread::spawn

join threads

use Barrier

test Mutex and/or atomics

Metrics

total runtime

average time per thread creation

average time per barrier

average time per synchronized update

Why this benchmark matters

This directly measures the runtime costs required by the project prompt.

Benchmark 2: Monte Carlo Pi
Purpose

Measure scalability and raw parallel performance on an embarrassingly parallel workload.

Workload characteristics

each thread performs independent random sampling

almost no synchronization except final reduction

OpenMP version

parallel loop with reduction

Rust version

split total samples across threads

accumulate local counts

combine at the end

Metrics

runtime

speedup

efficiency

Why this benchmark matters

This shows best-case multicore scaling with very little synchronization overhead.

Why we need Benchmark 2 and how it affects the decision flowchart

Benchmark 1 measured the cost of synchronization when there is almost no real work — empty parallel regions and barriers repeated millions of times. The results showed OpenMP is 5 to 16 times faster than Rust on fork/join and barrier overhead. However, that result alone is incomplete. It does not tell us whether OpenMP has an advantage when threads are doing meaningful work and rarely need to synchronize.

Benchmark 2 answers the opposite question. Each thread runs independently for a long time and only touches shared state once at the very end to combine its local hit count into the total. Synchronization cost is nearly zero relative to the compute time. This makes it the best-case scenario for parallel scaling.

The key question Benchmark 2 answers is: when synchronization is not the bottleneck, do the two languages scale equally well?

If they scale nearly identically on this workload, it confirms that the gap seen in Benchmark 1 was specifically about synchronization overhead and not about raw compute throughput or the quality of the compiler's parallel code generation. If one still outperforms the other here, it suggests a deeper difference worth investigating.

This result directly feeds into Q6 of the decision flowchart, which asks whether scalability is a primary concern. Q6A then splits on whether the bottleneck is synchronization overhead or load imbalance. Benchmark 2 provides the evidence for the synchronization overhead branch: if Rust and OpenMP converge on this workload, a decision maker whose system has coarse-grained parallel bodies and rare synchronization has no performance reason to prefer OpenMP over Rust, and should base the decision on other factors such as safety requirements, team background, and long-term maintainability.

Benchmark 2-1: Popcount Sum (Fairness Revision of Benchmark 2)
Purpose

Measure embarrassingly parallel scalability with pure integer work and no floating-point computation.

Why Benchmark 2-1 was added

Benchmark 2 (Monte Carlo Pi) was bottlenecked by Xorshift64's sequential bit-shift dependency chain, not the parallelism model. Switching to numerical integration (attempted briefly) revealed a second FP bottleneck: the scalar `divsd` instruction, which GCC -O3 did not vectorize due to the integer-to-float index conversion pattern. Both Pi approaches ultimately measured compiler behavior on specific FP loop patterns rather than parallel scaling quality.

Benchmark 2-1 removes all floating-point computation entirely. It computes:

  total = Σᵢ₌₀ᴺ⁻¹ popcount(i)

where popcount(i) counts the number of set bits in i. This maps to a single hardware instruction (`popcnt`) on x86. Both GCC and LLVM emit identical machine code for this operation, so any performance difference is purely from the threading model, not code generation.

Workload characteristics

no floating-point — pure integer, single hardware instruction per element

no RNG — fully deterministic, same result for the same N in both languages

no sequential state dependency between iterations — perfectly independent

uniform cost per element — schedule(static) is optimal

result is mathematically verifiable: for N = 2^k, expected = k × 2^(k-1)

for N = 2^33 = 8,589,934,592: expected total = 141,733,920,768

OpenMP version

parallel loop with reduction(+:total) and schedule(static)

N = 2^33 = 8,589,934,592

uses __builtin_popcountll → compiles to single `popcnt` instruction

Rust version

split range [0, N) across threads manually

each thread accumulates a private uint64 local sum

combine with fold at the end

uses i.count_ones() → compiles to single `popcnt` instruction

Metrics

runtime

speedup

efficiency

correctness (total_bits == expected_bits)

Benchmark 2-1 vs Benchmark 2 relationship

Benchmark 2 (Monte Carlo, Xorshift64) remains in the repository as a valid result: it demonstrates that RNG choice and compiler loop optimization can dominate a benchmark that appears to be about parallelism. The assembly analysis showing that LLVM used packed SSE2 mulpd while GCC was fully scalar is an important finding in its own right. Benchmark 2-1 is the corrected version that isolates parallelism scaling from compiler FP optimization.

Status

OpenMP version: complete (Benchmark2-1/cpp/openmp_benchmark2_1.cpp)

Rust version: pending

Results: pending (run on crunchy with run_benchmark2_1.sh)

Benchmark 3: Parallel Histogram or Dot Product
Purpose

Measure:

synchronization cost

shared/private variable handling

programmer control over data sharing

programmability for reduction-style workloads

Workload characteristics

multiple threads contribute to shared result

synchronization design matters

Possible choices
Option A: Dot product

simpler

easier to implement

good for reduction comparison

Option B: Histogram

more contention

better for studying synchronization strategies

Recommended choice

Use Histogram if time allows.
Use Dot Product if you want lower implementation risk.

Metrics

runtime

speedup

overhead caused by synchronization

difficulty of expressing shared/private data

Why this benchmark matters

This is a strong benchmark for comparing OpenMP reduction support against Rust’s explicit synchronization model.

Benchmark 4: Irregular Workload Benchmark
Purpose

Measure:

scheduling behavior

load balancing

programmer control

scalability under uneven workload

Possible choices
Option A: Prime testing over a large range

Different numbers may take different effort to test.

Option B: Mandelbrot set row computation

Different rows may have different cost.

Recommended choice

Use Prime testing because it is easier to implement and explain.

OpenMP version

compare schedule(static) and schedule(dynamic)

Rust version

manual work partitioning

static chunking

optional shared task queue if time allows

Metrics

runtime

speedup

behavior under load imbalance

programmer effort to manage scheduling

Why this benchmark matters

This benchmark helps evaluate “amount of control given to the programmer,” especially work distribution.

5. Comparison Criteria and Metrics
5.1 Performance Metrics

For each benchmark:

execution time

average runtime over multiple runs

speedup

efficiency

Formulas
Speedup
Speedup(n) = T(1) / T(n)
Efficiency
Efficiency(n) = Speedup(n) / n

Where:

T(1) = runtime with 1 thread

T(n) = runtime with n threads

5.2 Scalability Metrics

Use thread counts such as:

1, 2, 4, 8, 16, 32

Adjust the maximum thread count based on the crunchy node CPU core count.

We will observe:

how much performance improves as threads increase

where scaling begins to flatten

whether overhead dominates at high thread counts

5.3 Programmability Metrics

Programmability should not be discussed only subjectively.
We will record:

Lines of Code (LOC)

number of parallel constructs used

number of synchronization constructs used

time spent implementing each benchmark

debugging difficulty

clarity/readability of code

difficulty of expressing:

shared data

private data

reductions

synchronization

Suggested rubric

For each benchmark, rate each language from 1 to 5 on:

ease of writing

ease of understanding

ease of debugging

ease of modifying

ease of ensuring correctness

Also keep short notes such as:

“OpenMP version was shorter but more implicit.”

“Rust version required more code because ownership and sharing had to be explicit.”

5.4 Runtime Overhead Metrics

Especially for Benchmark 1, measure:

thread spawn cost

thread join cost

barrier cost

lock/mutex cost

atomic update cost if tested

This will help explain differences in fine-grained workloads.

5.5 Programmer Control Metrics

We will compare how much direct control the programmer has over:

number of threads

work scheduling

shared vs private variables

synchronization mechanism

affinity / binding if available

chunk size / task partitioning

OpenMP examples

OMP_NUM_THREADS

schedule(static)

schedule(dynamic)

private

shared

reduction

Rust examples

explicit thread creation

manual chunk assignment

explicit ownership and synchronization

atomics / mutex / barrier design

6. Experimental Environment
Hardware

Use one NYU CIMS crunchy node only for all experiments.

Do not spread work across multiple nodes.

Why

This project is about shared-memory multicore programming, not distributed systems.

7. Compilation and Execution Rules
OpenMP

Compile with optimization:

gcc -O3 -fopenmp benchmark.c -o benchmark

or for C++:

g++ -O3 -fopenmp benchmark.cpp -o benchmark
Rust

Compile in release mode:

cargo build --release
8. Benchmark Execution Procedure

For each benchmark:

choose a fixed input size

run with thread counts:

1, 2, 4, 8, 16, ...

repeat each configuration at least 5 times

record:

runtime

average runtime

speedup

efficiency

Notes

use large enough input sizes so the runtime is meaningful

avoid very tiny inputs, because overhead will dominate

try to run when the node is not heavily loaded

9. Suggested Input Sizes

These may be adjusted after pilot testing.

Benchmark 1: Thread overhead

enough repetitions to make timing stable

for example:

create/join threads 10,000 times

barrier 100,000 times

Benchmark 2: Monte Carlo Pi

10^8 samples or more

Benchmark 3: Histogram / Dot Product

arrays with tens or hundreds of millions of elements if memory allows

Benchmark 4: Prime testing

large integer range with enough work per thread

10. Project Schedule
Week 1: Setup and Rust Basics
Goals

set up toolchains on crunchy

learn only the Rust needed for this project

finalize benchmark definitions

build minimal test programs

Tasks

verify OpenMP compiler works

verify Rust compiler works

write:

OpenMP hello world

Rust thread hello world

learn Rust basics:

ownership

borrowing

vectors and slices

threads

Arc

Mutex

Barrier

create repository structure

implement Benchmark 1 first

Deliverables

environment ready

benchmark specs finalized

first benchmark started or finished

Week 2: Implement Benchmarks
Goals

Implement the main benchmark programs in both languages.

Tasks

finish Benchmark 1

implement Benchmark 2

implement Benchmark 3

validate correctness for each benchmark

begin collecting pilot timing results

Deliverables

at least 3 benchmark pairs completed

correctness checks done

initial timing available

Week 3: Finish Benchmarks and Run Full Experiments
Goals

implement final benchmark

run the full experiment matrix

collect all measurements

Tasks

finish Benchmark 4

run benchmarks with multiple thread counts

repeat each test multiple times

save results in CSV format

begin plotting graphs

Deliverables

all benchmark code complete

all experiment data collected

first draft of plots and tables

Week 4: Analysis and Report Writing
Goals

analyze results

write conclusions

produce usage guidelines

Tasks

compare OpenMP and Rust benchmark-by-benchmark

analyze:

programmability

scalability

overhead

control

performance

write final guidelines:

when OpenMP is better

when Rust is better

polish report

Deliverables

complete report

graphs and tables

final conclusions and recommendations

11. Repository Structure

Suggested structure:

project/
├── openmp/
│   ├── benchmark1_overhead/
│   ├── benchmark2_montecarlo/
│   ├── benchmark3_histogram/
│   └── benchmark4_irregular/
├── rust/
│   ├── benchmark1_overhead/
│   ├── benchmark2_montecarlo/
│   ├── benchmark3_histogram/
│   └── benchmark4_irregular/
├── scripts/
│   ├── run_openmp.sh
│   ├── run_rust.sh
│   └── collect_results.sh
├── results/
│   ├── raw/
│   └── processed/
└── report/
    └── project_report.md
12. Result Tables and Graphs to Produce
Tables

For each benchmark:

input size

thread count

average runtime

speedup

efficiency

For programmability:

LOC

synchronization constructs

implementation time

subjective difficulty notes

Graphs

Recommended graphs:

runtime vs thread count

speedup vs thread count

efficiency vs thread count

overhead comparison for synchronization/thread creation

LOC comparison across benchmarks

13. Expected Discussion Points

The final report should discuss tradeoffs such as:

OpenMP strengths

shorter code for loop parallelism

built-in reduction support

convenient scheduling controls

often lower friction for HPC-style shared-memory code

OpenMP weaknesses

shared/private behavior can be implicit

easier to write code with subtle race conditions

less compile-time safety

Rust strengths

strong safety guarantees

explicit ownership model

safer concurrency patterns

clearer explicit control over sharing

Rust weaknesses

steeper learning curve

more verbose parallel code

higher development effort for simple patterns

thread management may require more manual work

14. Expected Final Guidelines

The report should aim to answer:

When to use OpenMP

Use OpenMP when:

the workload is loop-based and data-parallel

performance tuning on shared-memory HPC code is the priority

fast development of classic multicore numeric code is needed

the team already works in C/C++

When to use Rust

Use Rust when:

safety and correctness are very important

concurrent code must remain maintainable over time

the programmer wants explicit control over ownership and synchronization

a higher initial development cost is acceptable for stronger safety guarantees

15. Risks and Mitigation
Risk 1: Rust learning curve is too steep

Mitigation: only learn the subset of Rust needed for these benchmarks.

Risk 2: benchmarks take too long to implement

Mitigation: prefer simple, controlled workloads over complicated real applications.

Risk 3: results are noisy on shared cluster nodes

Mitigation: repeat runs, average results, and use large workloads.

Risk 4: comparison becomes unfair

Mitigation: use the same algorithm, same input size, same node, and similar parallel structure.

16. Immediate Next Steps

confirm the exact four benchmarks

set up OpenMP and Rust on one crunchy node

implement Benchmark 1 first

create a CSV logging format for benchmark results

keep a small diary of implementation difficulty for programmability analysis

17. Final Benchmark Recommendation

To keep the project realistic and strong, use this final benchmark set:

Thread overhead microbenchmark

Monte Carlo Pi

Histogram
or Dot Product if you want a simpler reduction benchmark

Irregular prime testing

This set covers all major project requirements while remaining manageable in 3–4 weeks.

18. Final Project Statement

This project will perform a fair comparison between OpenMP and Rust on a shared-memory multicore system using four benchmarks that measure runtime overhead, scalability, programmability, programmer control, and overall performance. The final outcome will be a set of evidence-based guidelines describing the strengths and weaknesses of each model and when each should be preferred.

When you're ready, I can turn this into a cleaner report-style markdown version with title page, milestones, and checklist format.

19. Potential Concerns

Concern 1: Parallel Region Overhead vs True Thread Creation Cost

The OpenMP benchmark measures parallel region entry and exit overhead, not true OS thread creation. OpenMP implementations maintain a persistent thread pool: threads sleep between parallel regions and are woken up on entry rather than being created and destroyed. omp_set_dynamic(0) prevents dynamic thread count changes but does not disable thread pool reuse. The Rust side uses std::thread::spawn, which creates actual OS threads each time, making it structurally more expensive on this metric for reasons unrelated to language quality.

Mitigation: Do not label the OpenMP result as "thread creation cost." Label it explicitly as "parallel region entry overhead" in all tables and graphs, and add a paragraph in the report explaining that these measure different things: the natural unit of overhead in the OpenMP model is parallel region entry, while in Rust it is thread spawn. Both are valid measurements of their respective programming models.

Concern 2: Benchmark Design Favors Rust Verbosity

Excluding Rayon and requiring std::thread, Arc, Mutex, and Barrier directly means Rust code will always be more verbose than OpenMP for loop-parallel workloads. This is a legitimate methodological choice but it biases the programmability comparison. A reviewer could argue the comparison is between OpenMP and low-level Rust, not between OpenMP and idiomatic Rust.

Mitigation: State the choice clearly in the introduction and justify it: the goal is to compare the parallel programming model at a similar level of abstraction to OpenMP, not to compare convenience libraries.

Concern 3: Week 4 Timeline Is Tight

All four benchmarks in both languages are expected to be complete by the end of week 3, leaving only week 4 for analysis, graph production, and full report writing. If any benchmark takes longer than planned, report quality will suffer.

Mitigation: Prioritize finishing the Rust side of Benchmark 1 and Benchmark 2 before moving to Benchmarks 3 and 4. Begin writing the report structure and analysis sections in parallel with week 3 implementation work.

Concern 4: Programmability Rubric Is Subjective

The 1-to-5 ratings for ease of writing, understanding, debugging, modifying, and ensuring correctness are difficult to defend rigorously without objective anchors. Different team members may rate the same experience differently.

Mitigation: Anchor every rating with concrete evidence: lines of code counts, number of compiler errors or borrow checker errors encountered during development, number of iterations needed to get correct output, and short written notes explaining each rating. Do not assign ratings after the fact from memory.

Concern 5: Benchmark 3 Choice Is Still Open

The plan lists Histogram or Dot Product as alternatives but does not commit to one. Given the project is already underway, leaving this open creates scheduling risk and makes it harder to design the Rust side in advance.

Mitigation: Choose Histogram now if time permits, because it produces more interesting synchronization contention data and makes a stronger case for comparing OpenMP reduction support against Rust's explicit synchronization model. Use Dot Product only if implementation time becomes a hard constraint.

20. Backlog

Items that are known to be needed but are blocked or deferred.

Backlog Item 1: Clean 1-thread and 2-thread baseline for Benchmark 2

During Benchmark 2 OpenMP runs on crunchy2 and crunchy5, the 1-thread and 2-thread results were corrupted by interference from other users' jobs on the shared node. At low thread counts the benchmark occupies only 1-2 of 64 cores, leaving the rest available for the job scheduler to assign competing processes. This inflates runtime unpredictably and makes speedup calculations unreliable.

Action required: Re-run the 1-thread and 2-thread configurations during off-peak hours (early morning) on a quiet node. Confirm that all five trials fall within 10% of each other before accepting the baseline. Apply the same procedure to the Rust version of Benchmark 2 once implemented.

All Benchmark 2 speedup numbers in the report are provisional until this is resolved.