// Benchmark 3: Parallel Histogram — OpenMP Strategy A (private + merge)
//
// Each thread builds its own private histogram during compute.
// No shared writes, no contention during the parallel phase.
// At the end, the runtime merges all private copies into the final result.
//
// OpenMP expresses the entire private-copy + merge pattern with one clause:
//
//   #pragma omp parallel for reduction(+: h[:bins])
//
// The runtime automatically:
//   (1) creates a private zeroed copy of h[] for each thread
//   (2) lets each thread increment its own copy freely (no atomics)
//   (3) merges all copies into h[] after the parallel region ends
//
// This is the pattern that feeds Q5A in the decision flowchart:
//   "For reduction workloads, OpenMP expresses it in one clause;
//    Rust requires explicit private allocation and an explicit merge loop."
//
// Usage:
//   ./openmp_benchmark3 [threads] [n] [num_bins] [trials] [--csv] [--warmup]
//
// Examples:
//   ./openmp_benchmark3 8 67108864 256 5 --csv --warmup
//   ./openmp_benchmark3 1 67108864 256 5 --csv

#include <omp.h>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

static const long long DEFAULT_N       = 1LL << 26;  // 67,108,864 elements
static const int       DEFAULT_THREADS = 1;
static const int       DEFAULT_BINS    = 256;
static const int       DEFAULT_TRIALS  = 5;

struct Config {
    int       threads;
    long long n;
    int       num_bins;
    int       trials;
    bool      csv;
    bool      warmup;
};

Config parse_args(int argc, char* argv[]) {
    Config cfg = {DEFAULT_THREADS, DEFAULT_N, DEFAULT_BINS, DEFAULT_TRIALS, false, false};
    int pos = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--csv")    { cfg.csv    = true; continue; }
        if (arg == "--warmup") { cfg.warmup = true; continue; }
        switch (pos++) {
            case 0: cfg.threads  = std::stoi(arg);  break;
            case 1: cfg.n        = std::stoll(arg); break;
            case 2: cfg.num_bins = std::stoi(arg);  break;
            case 3: cfg.trials   = std::stoi(arg);  break;
        }
    }
    return cfg;
}

// Generate random input data using Xorshift64 (lower 32 bits).
// Generated once before all trials — not part of the timed region.
std::vector<uint32_t> generate_data(long long n) {
    std::vector<uint32_t> data(n);
    uint64_t state = 0xdeadbeefcafe1234ULL;
    for (long long i = 0; i < n; ++i) {
        state ^= state << 13;
        state ^= state >>  7;
        state ^= state << 17;
        data[i] = static_cast<uint32_t>(state);
    }
    return data;
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    const std::vector<uint32_t> data = generate_data(cfg.n);
    const uint32_t* d    = data.data();
    const long long n    = cfg.n;
    const int       bins = cfg.num_bins;

    // Output histogram — reused across trials, zeroed at start of each trial
    std::vector<long long> hist(bins);
    long long* h = hist.data();

    if (cfg.warmup) {
        const long long warmup_n = 1LL << 20;
        std::fill(h, h + bins, 0LL);
        #pragma omp parallel for num_threads(cfg.threads) schedule(static) \
                reduction(+: h[:bins])
        for (long long i = 0; i < warmup_n; ++i) {
            h[d[i] % bins]++;
        }
    }

    if (cfg.csv) {
        std::cout << "trial,threads,n,num_bins,elapsed_sec,correct\n";
    }

    for (int t = 1; t <= cfg.trials; ++t) {
        std::fill(h, h + bins, 0LL);

        double start = omp_get_wtime();

        // -----------------------------------------------------------------
        // Strategy A: OpenMP array reduction
        //
        // The reduction(+: h[:bins]) clause does three things invisibly:
        //   1. Each thread gets a private zeroed copy of h[]
        //   2. Each thread writes to its own copy — zero contention
        //   3. All copies are summed into h[] after the loop
        //
        // From the programmer's perspective: one clause, zero extra code.
        // -----------------------------------------------------------------
        #pragma omp parallel for num_threads(cfg.threads) schedule(static) \
                reduction(+: h[:bins])
        for (long long i = 0; i < n; ++i) {
            h[d[i] % bins]++;
        }

        double elapsed = omp_get_wtime() - start;

        // Verify: sum of all bins must equal N
        long long total = 0;
        for (int b = 0; b < bins; ++b) total += h[b];
        int correct = (total == n) ? 1 : 0;

        if (cfg.csv) {
            std::cout << t << "," << cfg.threads << "," << n << ","
                      << bins << "," << std::fixed << elapsed << ","
                      << correct << "\n";
        } else {
            std::cout << "Trial " << t << ": " << elapsed << "s"
                      << "  bins=" << bins
                      << "  correct=" << correct << "\n";
        }
    }

    return 0;
}
