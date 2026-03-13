#include <omp.h>

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

struct Config {
    int region_reps = 100000;
    int barrier_reps = 100000;
    int atomic_reps = 100000;
    int threads = 4;
};

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [threads] [region_reps] [barrier_reps] [atomic_reps]\n";
    std::cerr << "Example: " << prog << " 8 100000 100000 100000\n";
}

Config parse_args(int argc, char* argv[]) {
    Config cfg;

    if (argc >= 2) cfg.threads = std::atoi(argv[1]);
    if (argc >= 3) cfg.region_reps = std::atoi(argv[2]);
    if (argc >= 4) cfg.barrier_reps = std::atoi(argv[3]);
    if (argc >= 5) cfg.atomic_reps = std::atoi(argv[4]);

    if (cfg.threads <= 0 || cfg.region_reps <= 0 ||
        cfg.barrier_reps <= 0 || cfg.atomic_reps <= 0) {
        print_usage(argv[0]);
        std::exit(EXIT_FAILURE);
    }

    return cfg;
}

double benchmark_parallel_region(int repetitions, int threads) {
    double start = omp_get_wtime();

    for (int i = 0; i < repetitions; ++i) {
#pragma omp parallel num_threads(threads)
        {
            // Empty region on purpose
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

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);

    omp_set_dynamic(0);
    omp_set_num_threads(cfg.threads);

    std::cout << std::fixed << std::setprecision(9);
    std::cout << "=== OpenMP Overhead Benchmark ===\n";
    std::cout << "threads       : " << cfg.threads << "\n";
    std::cout << "region_reps   : " << cfg.region_reps << "\n";
    std::cout << "barrier_reps  : " << cfg.barrier_reps << "\n";
    std::cout << "atomic_reps   : " << cfg.atomic_reps << "\n\n";

    double region_time = benchmark_parallel_region(cfg.region_reps, cfg.threads);
    std::cout << "[Parallel Region]\n";
    std::cout << "total_time_sec          : " << region_time << "\n";
    std::cout << "avg_region_time_sec     : "
              << (region_time / cfg.region_reps) << "\n\n";

    double barrier_time = benchmark_barrier(cfg.barrier_reps, cfg.threads);
    std::cout << "[Barrier]\n";
    std::cout << "total_time_sec          : " << barrier_time << "\n";
    std::cout << "avg_barrier_time_sec    : "
              << (barrier_time / cfg.barrier_reps) << "\n\n";

    long long final_counter = 0;
    double atomic_time = benchmark_atomic(cfg.atomic_reps, cfg.threads, final_counter);
    long long expected = 1LL * cfg.threads * cfg.atomic_reps;

    std::cout << "[Atomic Increment]\n";
    std::cout << "total_time_sec          : " << atomic_time << "\n";
    std::cout << "avg_atomic_time_sec     : "
              << (atomic_time / expected) << "\n";
    std::cout << "final_counter           : " << final_counter << "\n";
    std::cout << "expected_counter        : " << expected << "\n";
    std::cout << "correct                 : "
              << (final_counter == expected ? "yes" : "no") << "\n";

    return 0;
}