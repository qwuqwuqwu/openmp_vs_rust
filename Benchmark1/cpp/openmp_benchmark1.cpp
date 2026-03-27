#include <omp.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

struct Config {
    int region_reps = 100000;
    int barrier_reps = 100000;
    int atomic_reps = 100000;
    int threads = 4;
    int trials = 5;
    bool csv = false;
    bool warmup = false;
};

struct Result {
    double region_time = 0.0;
    double barrier_time = 0.0;
    double atomic_time = 0.0;
    long long final_counter = 0;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [threads] [region_reps] [barrier_reps] [atomic_reps] [trials] [--csv] [--warmup]\n";
    std::cerr << "Example:\n";
    std::cerr << "  " << prog << " 8 100000 100000 100000 5\n";
    std::cerr << "  " << prog << " 8 100000 100000 100000 5 --csv\n";
    std::cerr << "  " << prog << " 8 100000 100000 100000 5 --csv --warmup\n";
}

Config parse_args(int argc, char* argv[]) {
    Config cfg;

    int numeric_arg_count = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--csv") {
            cfg.csv = true;
        } else if (arg == "--warmup") {
            cfg.warmup = true;
        } else {
            ++numeric_arg_count;
            int value = std::atoi(arg.c_str());
            if (numeric_arg_count == 1) cfg.threads = value;
            else if (numeric_arg_count == 2) cfg.region_reps = value;
            else if (numeric_arg_count == 3) cfg.barrier_reps = value;
            else if (numeric_arg_count == 4) cfg.atomic_reps = value;
            else if (numeric_arg_count == 5) cfg.trials = value;
            else {
                print_usage(argv[0]);
                std::exit(EXIT_FAILURE);
            }
        }
    }

    if (cfg.threads <= 0 || cfg.region_reps <= 0 ||
        cfg.barrier_reps <= 0 || cfg.atomic_reps <= 0 || cfg.trials <= 0) {
        print_usage(argv[0]);
        std::exit(EXIT_FAILURE);
    }

    return cfg;
}

double benchmark_parallel_region(int repetitions, int threads) {
    volatile int sink = 0;
    double start = omp_get_wtime();

    for (int i = 0; i < repetitions; ++i) {
#pragma omp parallel num_threads(threads)
        {
            sink = omp_get_thread_num(); // prevent region from being optimized away
        }
    }

    double end = omp_get_wtime();
    return end - start;
}

double benchmark_barrier(int repetitions, int threads) {
    double start = omp_get_wtime();

#pragma omp parallel num_threads(threads)
    {
        for (int i = 0; i < repetitions; ++i) {
#pragma omp barrier
        }
    }

    double end = omp_get_wtime();
    return end - start;
}

double benchmark_atomic(int repetitions, int threads, long long& final_counter) {
    long long counter = 0;

    double start = omp_get_wtime();

#pragma omp parallel num_threads(threads) shared(counter)
    {
        for (int i = 0; i < repetitions; ++i) {
#pragma omp atomic
            counter++;
        }
    }

    double end = omp_get_wtime();
    final_counter = counter;
    return end - start;
}

Result run_one_trial(const Config& cfg) {
    Result r;
    r.region_time = benchmark_parallel_region(cfg.region_reps, cfg.threads);
    r.barrier_time = benchmark_barrier(cfg.barrier_reps, cfg.threads);
    r.atomic_time = benchmark_atomic(cfg.atomic_reps, cfg.threads, r.final_counter);
    return r;
}

void run_warmup(const Config& cfg) {
    Result ignored = run_one_trial(cfg);
    (void)ignored;
}

void print_csv_header() {
    std::cout
        << "trial,threads,region_reps,barrier_reps,atomic_reps,"
        << "region_total_sec,region_avg_sec,"
        << "barrier_total_sec,barrier_avg_sec,"
        << "atomic_total_sec,atomic_avg_sec_per_increment,"
        << "final_counter,expected_counter,correct\n";
}

void print_csv_row(const Config& cfg, int trial, const Result& r) {
    long long expected = 1LL * cfg.threads * cfg.atomic_reps;
    double region_avg = r.region_time / cfg.region_reps;
    double barrier_avg = r.barrier_time / cfg.barrier_reps;
    double atomic_avg = r.atomic_time / expected;
    int correct = (r.final_counter == expected) ? 1 : 0;

    std::cout << std::fixed << std::setprecision(9);
    std::cout
        << trial << ","
        << cfg.threads << ","
        << cfg.region_reps << ","
        << cfg.barrier_reps << ","
        << cfg.atomic_reps << ","
        << r.region_time << ","
        << region_avg << ","
        << r.barrier_time << ","
        << barrier_avg << ","
        << r.atomic_time << ","
        << atomic_avg << ","
        << r.final_counter << ","
        << expected << ","
        << correct << "\n";
}

void print_human_readable(const Config& cfg, int trial, const Result& r) {
    long long expected = 1LL * cfg.threads * cfg.atomic_reps;

    std::cout << std::fixed << std::setprecision(9);
    std::cout << "=== OpenMP Overhead Benchmark ===\n";
    std::cout << "trial         : " << trial << "\n";
    std::cout << "threads       : " << cfg.threads << "\n";
    std::cout << "region_reps   : " << cfg.region_reps << "\n";
    std::cout << "barrier_reps  : " << cfg.barrier_reps << "\n";
    std::cout << "atomic_reps   : " << cfg.atomic_reps << "\n\n";

    std::cout << "[Parallel Region]\n";
    std::cout << "total_time_sec          : " << r.region_time << "\n";
    std::cout << "avg_region_time_sec     : "
              << (r.region_time / cfg.region_reps) << "\n\n";

    std::cout << "[Barrier]\n";
    std::cout << "total_time_sec          : " << r.barrier_time << "\n";
    std::cout << "avg_barrier_time_sec    : "
              << (r.barrier_time / cfg.barrier_reps) << "\n\n";

    std::cout << "[Atomic Increment]\n";
    std::cout << "total_time_sec          : " << r.atomic_time << "\n";
    std::cout << "avg_atomic_time_sec     : "
              << (r.atomic_time / expected) << "\n";
    std::cout << "final_counter           : " << r.final_counter << "\n";
    std::cout << "expected_counter        : " << expected << "\n";
    std::cout << "correct                 : "
              << (r.final_counter == expected ? "yes" : "no") << "\n\n";
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    omp_set_dynamic(0);
    omp_set_num_threads(cfg.threads);

    if (cfg.warmup) {
        run_warmup(cfg);
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