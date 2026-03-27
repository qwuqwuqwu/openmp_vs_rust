#include <omp.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

// Monte Carlo Pi estimation.
//
// Each thread independently throws darts at the unit square [0,1) x [0,1).
// A dart is a "hit" if x^2 + y^2 < 1.0 (i.e. it lands inside the unit circle).
// pi ~= 4 * hits / total_throws
//
// Parallelism structure:
//   - The total sample count is split evenly across threads via a parallel
//     reduction loop.
//   - Each thread uses its own seeded mt19937 RNG to avoid contention on a
//     shared generator and to make results reproducible per thread count.
//   - The only shared-state operation is the final reduction of hit counts,
//     expressed as a single OpenMP reduction clause.
//
// This is intentionally embarrassingly parallel: threads share no state during
// sampling. Synchronization cost is limited to one reduction at the end.

struct Config {
    long long samples  = 100000000LL; // total dart throws
    int       threads  = 4;
    int       trials   = 5;
    bool      csv      = false;
    bool      warmup   = false;
};

struct Result {
    double pi_estimate = 0.0;
    double elapsed_sec = 0.0;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [threads] [samples] [trials] [--csv] [--warmup]\n";
    std::cerr << "Example:\n";
    std::cerr << "  " << prog << " 4 100000000 5\n";
    std::cerr << "  " << prog << " 4 100000000 5 --csv\n";
    std::cerr << "  " << prog << " 4 100000000 5 --csv --warmup\n";
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
            else if (numeric_count == 2) cfg.samples = value;
            else if (numeric_count == 3) cfg.trials  = (int)value;
            else {
                print_usage(argv[0]);
                std::exit(EXIT_FAILURE);
            }
        }
    }

    if (cfg.threads <= 0 || cfg.samples <= 0 || cfg.trials <= 0) {
        print_usage(argv[0]);
        std::exit(EXIT_FAILURE);
    }

    return cfg;
}

Result run_one_trial(const Config& cfg) {
    long long hits = 0;

    double start = omp_get_wtime();

#pragma omp parallel reduction(+:hits) num_threads(cfg.threads)
    {
        // Each thread gets its own RNG seeded by thread id so results are
        // deterministic and threads never share generator state.
        int tid = omp_get_thread_num();
        std::mt19937_64 rng(42 + tid);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        long long local_hits = 0;

#pragma omp for schedule(static)
        for (long long i = 0; i < cfg.samples; ++i) {
            double x = dist(rng);
            double y = dist(rng);
            if (x * x + y * y < 1.0) {
                ++local_hits;
            }
        }

        hits += local_hits;
    }

    double end = omp_get_wtime();

    Result r;
    r.pi_estimate = 4.0 * (double)hits / (double)cfg.samples;
    r.elapsed_sec = end - start;
    return r;
}

void print_csv_header() {
    std::cout << "trial,threads,samples,elapsed_sec,pi_estimate,pi_error\n";
}

void print_csv_row(const Config& cfg, int trial, const Result& r) {
    double error = r.pi_estimate - 3.14159265358979323846;
    std::cout << std::fixed << std::setprecision(9);
    std::cout << trial << ","
              << cfg.threads << ","
              << cfg.samples << ","
              << r.elapsed_sec << ","
              << r.pi_estimate << ","
              << error << "\n";
}

void print_human_readable(const Config& cfg, int trial, const Result& r) {
    double error = r.pi_estimate - 3.14159265358979323846;
    std::cout << std::fixed << std::setprecision(9);
    std::cout << "=== OpenMP Monte Carlo Pi Benchmark ===\n";
    std::cout << "trial         : " << trial << "\n";
    std::cout << "threads       : " << cfg.threads << "\n";
    std::cout << "samples       : " << cfg.samples << "\n\n";
    std::cout << "elapsed_sec   : " << r.elapsed_sec << "\n";
    std::cout << "pi_estimate   : " << r.pi_estimate << "\n";
    std::cout << "pi_error      : " << error << "\n\n";
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    omp_set_dynamic(0);
    omp_set_num_threads(cfg.threads);

    if (cfg.warmup) {
        Config warmup_cfg = cfg;
        warmup_cfg.samples = 1000000;
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
