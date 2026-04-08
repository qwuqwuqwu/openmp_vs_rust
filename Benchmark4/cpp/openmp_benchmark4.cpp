// Benchmark 4: Irregular Workload — Prime Testing (OpenMP)
//
// Count all primes in [2, N] using trial division.
//
// Why this workload is irregular:
//   Checking whether n is prime requires trial division up to √n.
//   - Small composites (e.g. n=4): 1 division, done instantly.
//   - Large primes (e.g. n=999,983): ~1000 divisions before confirming prime.
//   Cost per element varies by orders of magnitude across the range.
//   Large primes are denser at the top of the range, so the last static
//   chunk always gets the heaviest work — static partitioning bottlenecks.
//
// Two schedules compared in this benchmark:
//
//   schedule(static)  — fixed chunks assigned upfront.
//                       Thread T-1 gets [N*(T-1)/T .. N], which is the
//                       densest region. All other threads idle while it works.
//
//   schedule(dynamic, chunk_size) — idle threads grab the next available
//                       chunk from a shared queue at runtime. Load naturally
//                       balances because fast threads pick up more chunks.
//
// Both are controlled via the CLI "schedule" argument so the same binary
// produces both datasets.
//
// Usage:
//   ./openmp_benchmark4 [threads] [n] [schedule] [chunk_size] [trials] [--csv] [--warmup]
//   schedule: "static" or "dynamic"
//
// Examples:
//   ./openmp_benchmark4 8 1000000 static  0   5 --csv --warmup
//   ./openmp_benchmark4 8 1000000 dynamic 100 5 --csv --warmup

#include <omp.h>
#include <cmath>
#include <iostream>
#include <string>

static const long long DEFAULT_N          = 1000000LL;  // test primes in [2, 1,000,000]
static const int       DEFAULT_THREADS    = 1;
static const int       DEFAULT_CHUNK_SIZE = 100;
static const int       DEFAULT_TRIALS     = 5;
static const long long EXPECTED_PRIMES    = 78498LL;    // π(1,000,000) = 78,498

struct Config {
    int       threads;
    long long n;
    bool      is_dynamic;   // true = schedule(dynamic), false = schedule(static)
    int       chunk_size;   // only used when is_dynamic = true
    int       trials;
    bool      csv;
    bool      warmup;
};

Config parse_args(int argc, char* argv[]) {
    Config cfg = {DEFAULT_THREADS, DEFAULT_N, false, DEFAULT_CHUNK_SIZE, DEFAULT_TRIALS, false, false};
    int pos = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--csv")    { cfg.csv    = true; continue; }
        if (arg == "--warmup") { cfg.warmup = true; continue; }
        switch (pos++) {
            case 0: cfg.threads    = std::stoi(arg); break;
            case 1: cfg.n          = std::stoll(arg); break;
            case 2: cfg.is_dynamic = (arg == "dynamic"); break;
            case 3: cfg.chunk_size = std::stoi(arg); break;
            case 4: cfg.trials     = std::stoi(arg); break;
        }
    }
    return cfg;
}

// Trial division primality test.
// Returns true if n is prime.
// Cost: O(√n) divisions in the worst case (n is prime).
// This is the source of load imbalance — large primes are expensive.
inline bool is_prime(long long n) {
    if (n < 2)      return false;
    if (n == 2)     return true;
    if (n % 2 == 0) return false;
    for (long long i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

// Run one trial with the configured schedule.
// Uses omp_set_schedule + schedule(runtime) so one loop body serves both modes.
long long run_trial(const Config& cfg) {
    // Set the OpenMP runtime schedule before entering the parallel region
    if (cfg.is_dynamic) {
        omp_set_schedule(omp_sched_dynamic, cfg.chunk_size);
    } else {
        omp_set_schedule(omp_sched_static, 0);
    }

    long long count = 0;

    // schedule(runtime) reads the schedule set above via omp_set_schedule.
    // Switching between static and dynamic requires no code change here —
    // only the omp_set_schedule call above changes.
    #pragma omp parallel for num_threads(cfg.threads) schedule(runtime) \
            reduction(+:count)
    for (long long i = 2; i <= cfg.n; ++i) {
        if (is_prime(i)) count++;
    }

    return count;
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    const std::string sched_name = cfg.is_dynamic ? "dynamic" : "static";

    if (cfg.warmup) {
        // Small warmup run to wake up the OpenMP thread pool
        Config warm = cfg;
        warm.n = 10000;
        run_trial(warm);
    }

    if (cfg.csv) {
        std::cout << "trial,threads,n,schedule,chunk_size,elapsed_sec,prime_count,correct\n";
    }

    for (int t = 1; t <= cfg.trials; ++t) {
        double start = omp_get_wtime();
        long long prime_count = run_trial(cfg);
        double elapsed = omp_get_wtime() - start;

        int correct = (prime_count == EXPECTED_PRIMES) ? 1 : 0;

        if (cfg.csv) {
            std::cout << t << ","
                      << cfg.threads << ","
                      << cfg.n << ","
                      << sched_name << ","
                      << (cfg.is_dynamic ? cfg.chunk_size : 0) << ","
                      << std::fixed << elapsed << ","
                      << prime_count << ","
                      << correct << "\n";
        } else {
            std::cout << "Trial " << t << ": " << elapsed << "s"
                      << "  schedule=" << sched_name
                      << "  primes=" << prime_count
                      << "  correct=" << correct << "\n";
        }
    }

    return 0;
}
