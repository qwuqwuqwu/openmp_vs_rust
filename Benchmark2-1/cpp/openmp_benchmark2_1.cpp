#include <omp.h>

#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

// Popcount Sum Benchmark.
//
// Computes the total number of set bits across all integers from 0 to N-1:
//
//   total = Σᵢ₌₀ᴺ⁻¹ popcount(i)
//
// Each popcount(i) is fully independent — no state is carried between
// iterations. The compiler maps __builtin_popcountll directly to the
// single-cycle hardware `popcnt` instruction on x86. Both GCC and LLVM
// emit identical machine code for this, so any performance difference
// between OpenMP and Rust comes purely from the parallelism model.
//
// Why this benchmark:
//   Previous Pi benchmarks (Monte Carlo and numerical integration) were
//   bottlenecked by floating-point operations: Xorshift64's bit-shift
//   dependency chain in Benchmark 2, and the scalar `divsd` instruction
//   in Benchmark 2-1 (numerical integration). Even with -O3, GCC did not
//   vectorize either inner loop.
//
//   Popcount removes all FP computation. The inner loop is:
//     local += __builtin_popcountll(i)
//   which compiles to a single `popcnt` instruction per iteration.
//   There is no division, no RNG, no FP, and no sequential state dependency.
//   Both compilers generate identical scalar inner loops, making the
//   comparison a clean test of parallel scaling with uniform integer work.
//
// Correctness verification:
//   For any N that is a power of 2, N = 2^k:
//     expected = k * 2^(k-1)
//   For N = 2^33 = 8,589,934,592:
//     expected = 33 * 2^32 = 141,733,920,768
//
// Parallelism structure:
//   - The range [0, N) is divided statically across threads.
//   - Each thread accumulates a private uint64_t local sum.
//   - A single OpenMP reduction combines all thread results at the end.
//   - No synchronization during the computation phase.

static const long long DEFAULT_N = (1LL << 33); // 8,589,934,592

// Expected answer for N = 2^k: k * 2^(k-1)
// For N = 2^33: 33 * 4,294,967,296 = 141,733,920,768
static const uint64_t EXPECTED_BITS = 141733920768ULL;

struct Config {
    long long n       = DEFAULT_N;
    int       threads = 4;
    int       trials  = 5;
    bool      csv     = false;
    bool      warmup  = false;
};

struct Result {
    uint64_t total_bits = 0;
    double   elapsed_sec = 0.0;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [threads] [n] [trials] [--csv] [--warmup]\n";
    std::cerr << "  n        : total integers to process (default 2^33 = 8589934592)\n";
    std::cerr << "Example:\n";
    std::cerr << "  " << prog << " 4 8589934592 5 --csv --warmup\n";
}

Config parse_args(int argc, char* argv[]) {
    Config cfg;
    int numeric_count = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--csv") {
            cfg.csv = true;
        } else if (arg == "--warmup") {
            cfg.warmup = true;
        } else {
            ++numeric_count;
            long long value = std::atoll(arg.c_str());
            if      (numeric_count == 1) cfg.threads = (int)value;
            else if (numeric_count == 2) cfg.n       = value;
            else if (numeric_count == 3) cfg.trials  = (int)value;
            else {
                print_usage(argv[0]);
                std::exit(EXIT_FAILURE);
            }
        }
    }

    if (cfg.threads <= 0 || cfg.n <= 0 || cfg.trials <= 0) {
        print_usage(argv[0]);
        std::exit(EXIT_FAILURE);
    }

    return cfg;
}

Result run_one_trial(const Config& cfg) {
    uint64_t total = 0;

    double start = omp_get_wtime();

    // Each thread accumulates into a private local sum.
    // schedule(static) divides [0, N) into equal contiguous chunks — correct
    // because every popcount(i) takes the same time regardless of i.
#pragma omp parallel reduction(+:total) num_threads(cfg.threads)
    {
        uint64_t local = 0;

#pragma omp for schedule(static)
        for (long long i = 0; i < cfg.n; ++i) {
            local += (uint64_t)__builtin_popcountll((unsigned long long)i);
        }

        total += local;
    }

    double end = omp_get_wtime();

    Result r;
    r.total_bits  = total;
    r.elapsed_sec = end - start;
    return r;
}

void print_csv_header() {
    std::cout << "trial,threads,n,elapsed_sec,total_bits,expected_bits,correct\n";
}

void print_csv_row(const Config& cfg, int trial, const Result& r) {
    uint64_t expected = (cfg.n == DEFAULT_N) ? EXPECTED_BITS : 0;
    int correct = (expected == 0) ? -1 : (r.total_bits == expected ? 1 : 0);
    std::cout << std::fixed << std::setprecision(12);
    std::cout << trial        << ","
              << cfg.threads  << ","
              << cfg.n        << ","
              << r.elapsed_sec << ","
              << r.total_bits  << ","
              << expected      << ","
              << correct       << "\n";
}

void print_human_readable(const Config& cfg, int trial, const Result& r) {
    uint64_t expected = (cfg.n == DEFAULT_N) ? EXPECTED_BITS : 0;
    bool correct = (expected > 0) && (r.total_bits == expected);
    std::cout << std::fixed << std::setprecision(12);
    std::cout << "=== OpenMP Popcount Sum Benchmark ===\n";
    std::cout << "trial         : " << trial            << "\n";
    std::cout << "threads       : " << cfg.threads      << "\n";
    std::cout << "n             : " << cfg.n            << "\n\n";
    std::cout << "elapsed_sec   : " << r.elapsed_sec    << "\n";
    std::cout << "total_bits    : " << r.total_bits     << "\n";
    if (expected > 0) {
        std::cout << "expected_bits : " << expected         << "\n";
        std::cout << "correct       : " << (correct ? "YES" : "NO") << "\n";
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    omp_set_dynamic(0);
    omp_set_num_threads(cfg.threads);

    if (cfg.warmup) {
        Config warmup_cfg = cfg;
        warmup_cfg.n = 1 << 20; // 1M items for warmup
        run_one_trial(warmup_cfg);
    }

    if (cfg.csv) {
        print_csv_header();
    }

    for (int trial = 1; trial <= cfg.trials; ++trial) {
        Result r = run_one_trial(cfg);
        if (cfg.csv) {
            print_csv_row(cfg, trial, r);
        } else {
            print_human_readable(cfg, trial, r);
        }
    }

    return 0;
}
