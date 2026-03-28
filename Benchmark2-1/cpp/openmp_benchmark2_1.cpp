#include <omp.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

// Numerical Integration Pi estimation (Midpoint Rule).
//
// Approximates π using the identity:
//   π = ∫₀¹ 4 / (1 + x²) dx
//
// The interval [0, 1] is divided into N equal subintervals.
// Each subinterval i uses the midpoint x_i = (i + 0.5) / N:
//   π ≈ Σᵢ₌₀ᴺ⁻¹ [ 4 / (1 + xᵢ²) ] × (1/N)
//
// Parallelism structure:
//   - The N subintervals are divided statically across threads via a
//     parallel reduction loop.
//   - Each thread accumulates a private partial sum with no shared writes
//     during computation.
//   - A single OpenMP reduction combines all partial sums at the end.
//   - No RNG, no shared state, no sequential state dependency — fully
//     deterministic and freely vectorizable.
//
// Why this benchmark:
//   Benchmark 2 (Monte Carlo Pi) used Xorshift64 to generate random
//   samples. Even with the same RNG on both sides, the sequential
//   bit-shift dependency chain inside Xorshift64 became the real
//   bottleneck — making the result sensitive to how GCC vs LLVM compiled
//   that specific loop, not to the parallelism model.
//
//   This benchmark removes the RNG entirely. The inner loop is:
//     x = (i + 0.5) * h
//     sum += 4.0 / (1.0 + x * x)
//   Each iteration is fully independent with no state carry-over.
//   Both compilers can apply AVX2 auto-vectorization freely, making the
//   comparison a fair test of parallel scaling rather than compiler
//   optimization of a specific RNG pattern.

static const double TRUE_PI = 3.14159265358979323846;

struct Config {
    long long intervals = 1000000000LL;
    int       threads   = 4;
    int       trials    = 5;
    bool      csv       = false;
    bool      warmup    = false;
};

struct Result {
    double pi_estimate = 0.0;
    double elapsed_sec = 0.0;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [threads] [intervals] [trials] [--csv] [--warmup]\n";
    std::cerr << "Example:\n";
    std::cerr << "  " << prog << " 4 1000000000 5 --csv --warmup\n";
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
            if      (numeric_count == 1) cfg.threads   = (int)value;
            else if (numeric_count == 2) cfg.intervals = value;
            else if (numeric_count == 3) cfg.trials    = (int)value;
            else {
                print_usage(argv[0]);
                std::exit(EXIT_FAILURE);
            }
        }
    }

    if (cfg.threads <= 0 || cfg.intervals <= 0 || cfg.trials <= 0) {
        print_usage(argv[0]);
        std::exit(EXIT_FAILURE);
    }

    return cfg;
}

Result run_one_trial(const Config& cfg) {
    double sum = 0.0;
    double h   = 1.0 / (double)cfg.intervals;

    double start = omp_get_wtime();

    // Each thread accumulates a private local_sum, combined via reduction.
    // schedule(static) gives equal contiguous chunks — correct for uniform
    // cost per interval (all evaluations of 4/(1+x²) take the same time).
#pragma omp parallel reduction(+:sum) num_threads(cfg.threads)
    {
        double local_sum = 0.0;

#pragma omp for schedule(static)
        for (long long i = 0; i < cfg.intervals; ++i) {
            double x = (i + 0.5) * h;
            local_sum += 4.0 / (1.0 + x * x);
        }

        sum += local_sum;
    }

    double pi  = sum * h;
    double end = omp_get_wtime();

    Result r;
    r.pi_estimate = pi;
    r.elapsed_sec = end - start;
    return r;
}

void print_csv_header() {
    std::cout << "trial,threads,intervals,elapsed_sec,pi_estimate,pi_error\n";
}

void print_csv_row(const Config& cfg, int trial, const Result& r) {
    double error = r.pi_estimate - TRUE_PI;
    std::cout << std::fixed << std::setprecision(12);
    std::cout << trial       << ","
              << cfg.threads << ","
              << cfg.intervals << ","
              << r.elapsed_sec << ","
              << r.pi_estimate << ","
              << error         << "\n";
}

void print_human_readable(const Config& cfg, int trial, const Result& r) {
    double error = r.pi_estimate - TRUE_PI;
    std::cout << std::fixed << std::setprecision(12);
    std::cout << "=== OpenMP Numerical Integration Pi Benchmark ===\n";
    std::cout << "trial         : " << trial           << "\n";
    std::cout << "threads       : " << cfg.threads     << "\n";
    std::cout << "intervals     : " << cfg.intervals   << "\n\n";
    std::cout << "elapsed_sec   : " << r.elapsed_sec   << "\n";
    std::cout << "pi_estimate   : " << r.pi_estimate   << "\n";
    std::cout << "pi_error      : " << error           << "\n\n";
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    omp_set_dynamic(0);
    omp_set_num_threads(cfg.threads);

    if (cfg.warmup) {
        Config warmup_cfg    = cfg;
        warmup_cfg.intervals = 1000000;
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
