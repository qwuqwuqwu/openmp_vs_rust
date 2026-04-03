// Benchmark 3: Parallel Histogram — OpenMP Atomic (Strategy B)
//
// All threads write to a single shared histogram array during compute.
// Each write is protected by #pragma omp atomic, which compiles to a
// hardware `lock add` instruction — same cost as Rust's AtomicU64::fetch_add.
//
// This is fundamentally different from Benchmarks 2/2-1:
//   - Threads contend on shared state DURING compute, not just at the end
//   - Contention level is controlled by num_bins:
//       few bins  → many threads collide → high contention
//       many bins → threads rarely collide → low contention
//
// Usage:
//   ./openmp_benchmark3 [threads] [n] [num_bins] [trials] [--csv] [--warmup]
//
// Examples:
//   ./openmp_benchmark3 8 67108864 16   5 --csv --warmup   # high contention
//   ./openmp_benchmark3 8 67108864 1024 5 --csv --warmup   # low contention

#include <omp.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static const long long DEFAULT_N        = 1LL << 26;  // 67,108,864 elements
static const int       DEFAULT_THREADS  = 1;
static const int       DEFAULT_BINS     = 256;
static const int       DEFAULT_TRIALS   = 5;

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

// Generate random input data.
// Uses Xorshift64 to produce a 64-bit random state each iteration,
// then takes the lower 32 bits as the stored value.
// 32 bits is sufficient since we only need data[i] % num_bins later.
// Data generation is NOT part of the timed region — generated once and reused.
std::vector<uint32_t> generate_data(long long n) {
    std::vector<uint32_t> data(n);
    uint64_t state = 0xdeadbeefcafe1234ULL;
    for (long long i = 0; i < n; ++i) {
        // Xorshift64: update 64-bit state
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        // Store lower 32 bits as the data element
        data[i] = static_cast<uint32_t>(state);
    }
    return data;
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    // Generate input once — outside all timing
    const std::vector<uint32_t> data = generate_data(cfg.n);
    const uint32_t* d    = data.data();
    const long long n    = cfg.n;
    const int       bins = cfg.num_bins;

    // Shared histogram — reused across trials (zeroed at start of each trial)
    std::vector<long long> hist(bins);
    long long* h = hist.data();

    if (cfg.warmup) {
        // One warmup pass at reduced size to wake up the OpenMP thread pool
        const long long warmup_n = 1 << 20;
        std::fill(h, h + bins, 0LL);
        #pragma omp parallel for num_threads(cfg.threads) schedule(static)
        for (long long i = 0; i < warmup_n; ++i) {
            int bucket = d[i % n] % bins;
            #pragma omp atomic
            h[bucket]++;
        }
    }

    if (cfg.csv) {
        std::cout << "trial,threads,n,num_bins,elapsed_sec,correct\n";
    }

    for (int t = 1; t <= cfg.trials; ++t) {
        // Zero the histogram — included in timed region (part of real work)
        std::fill(h, h + bins, 0LL);

        double start = omp_get_wtime();

        // ---------------------------------------------------------------
        // Strategy B: single shared histogram, atomic update per element
        // Each thread reads d[i], computes bucket, atomically increments
        // hist[bucket]. No private copies — all contention happens here.
        // ---------------------------------------------------------------
        #pragma omp parallel for num_threads(cfg.threads) schedule(static)
        for (long long i = 0; i < n; ++i) {
            int bucket = static_cast<int>(d[i] % static_cast<uint32_t>(bins));
            #pragma omp atomic
            h[bucket]++;
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
