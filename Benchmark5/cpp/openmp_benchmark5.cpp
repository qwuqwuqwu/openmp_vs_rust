/*
 * Benchmark 5: Thread-to-Core Affinity
 * Workload: parallel sum of a large uint64_t array (memory-bandwidth-bound)
 *
 * Affinity strategies (selected at runtime via argv[3]):
 *   "default" — no proc_bind clause; OS decides thread placement
 *   "spread"  — proc_bind(spread): threads spread across available NUMA nodes
 *   "close"   — proc_bind(close):  threads packed into fewest NUMA nodes
 *
 * KEY DESIGN NOTE:
 *   proc_bind is a *compile-time pragma clause* — it cannot be passed as a
 *   runtime variable. Therefore three separate parallel regions are required,
 *   one per strategy. The runtime branch (strategy string) selects which
 *   region executes. Code duplication is minimal: the loop body is identical.
 *
 * FIRST-TOUCH POLICY:
 *   Linux allocates each memory page on the NUMA node of the thread that first
 *   writes it. The array is therefore initialized *in parallel with the same
 *   affinity as the compute phase*, so each thread's chunk is placed on its
 *   local NUMA node. Without this, all pages would land on NUMA node 0.
 *
 * LOC for affinity control (OpenMP):
 *   + proc_bind(spread) → 1 keyword added to existing pragma
 *   + proc_bind(close)  → 1 keyword added to existing pragma
 *   No new functions, no new dependencies, no env-var coordination needed.
 *
 * Usage:
 *   ./openmp_benchmark5 [threads] [n] [strategy] [trials] [--csv] [--warmup]
 *   e.g.: ./openmp_benchmark5 32 134217728 spread 5 --csv --warmup
 */

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

/* Default: 128M × 8 bytes = 1 GB — well above per-NUMA-node L3 (16 MB) */
#define N_DEFAULT (128LL * 1024 * 1024)

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ── Initialization: first-touch with each strategy ─────────────────────── */

static void init_default(uint64_t *arr, long long n, int T) {
    #pragma omp parallel for num_threads(T) schedule(static)
    for (long long i = 0; i < n; i++) arr[i] = (uint64_t)i;
}

static void init_spread(uint64_t *arr, long long n, int T) {
    #pragma omp parallel for proc_bind(spread) num_threads(T) schedule(static)
    for (long long i = 0; i < n; i++) arr[i] = (uint64_t)i;
}

static void init_close(uint64_t *arr, long long n, int T) {
    #pragma omp parallel for proc_bind(close) num_threads(T) schedule(static)
    for (long long i = 0; i < n; i++) arr[i] = (uint64_t)i;
}

/* ── Compute: parallel reduction with each strategy ─────────────────────── */
/*
 * Three regions are required because proc_bind is a compile-time clause.
 * Runtime selection is done via the if/else in main().
 * Programmer effort per strategy: 1 word ("spread" / "close") in the pragma.
 */

static uint64_t sum_default(uint64_t *arr, long long n, int T) {
    uint64_t s = 0;
    #pragma omp parallel for reduction(+:s) num_threads(T) schedule(static)
    for (long long i = 0; i < n; i++) s += arr[i];
    return s;
}

static uint64_t sum_spread(uint64_t *arr, long long n, int T) {
    uint64_t s = 0;
    /* ↓ the only difference from sum_default: proc_bind(spread) */
    #pragma omp parallel for proc_bind(spread) reduction(+:s) num_threads(T) schedule(static)
    for (long long i = 0; i < n; i++) s += arr[i];
    return s;
}

static uint64_t sum_close(uint64_t *arr, long long n, int T) {
    uint64_t s = 0;
    /* ↓ the only difference from sum_default: proc_bind(close) */
    #pragma omp parallel for proc_bind(close) reduction(+:s) num_threads(T) schedule(static)
    for (long long i = 0; i < n; i++) s += arr[i];
    return s;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    int         threads  = 8;
    long long   n        = N_DEFAULT;
    const char *strategy = "default";
    int         trials   = 5;
    int         warmup   = 0;
    int         csv      = 0;

    /* Positional args: threads  n  strategy  trials */
    int pos = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--csv")    == 0) csv    = 1;
        else if (strcmp(argv[i], "--warmup") == 0) warmup = 1;
        else {
            switch (++pos) {
                case 1: threads  = atoi(argv[i]);   break;
                case 2: n        = atoll(argv[i]);  break;
                case 3: strategy = argv[i];          break;
                case 4: trials   = atoi(argv[i]);   break;
            }
        }
    }

    /* Validate strategy */
    int is_spread  = strcmp(strategy, "spread")  == 0;
    int is_close   = strcmp(strategy, "close")   == 0;
    int is_default = strcmp(strategy, "default") == 0;
    if (!is_spread && !is_close && !is_default) {
        fprintf(stderr, "Unknown strategy '%s'. Use: default | spread | close\n", strategy);
        return 1;
    }

    /* Allocate (uninitialized — first-touch done in parallel below) */
    uint64_t *arr = (uint64_t *)malloc((size_t)n * sizeof(uint64_t));
    if (!arr) { fprintf(stderr, "malloc failed for %lld elements\n", n); return 1; }

    /*
     * Parallel first-touch initialization.
     * Each thread writes its static chunk → pages land on the thread's
     * local NUMA node. Subsequent reads in the same strategy access local memory.
     */
    if      (is_spread)  init_spread (arr, n, threads);
    else if (is_close)   init_close  (arr, n, threads);
    else                 init_default(arr, n, threads);

    /* Optional warmup (not timed) */
    if (warmup) {
        if      (is_spread)  sum_spread (arr, n, threads);
        else if (is_close)   sum_close  (arr, n, threads);
        else                 sum_default(arr, n, threads);
    }

    if (csv)
        printf("trial,threads,n,strategy,elapsed_sec,bandwidth_GBs,checksum\n");

    double bytes = (double)n * sizeof(uint64_t);

    for (int t = 1; t <= trials; t++) {
        double   t0;
        uint64_t result;

        t0 = now_sec();
        if      (is_spread)  result = sum_spread (arr, n, threads);
        else if (is_close)   result = sum_close  (arr, n, threads);
        else                 result = sum_default(arr, n, threads);
        double elapsed = now_sec() - t0;

        double bw_gbs = bytes / elapsed / 1e9;

        if (csv)
            printf("%d,%d,%lld,%s,%.9f,%.3f,%llu\n",
                   t, threads, n, strategy, elapsed, bw_gbs, (unsigned long long)result);
        else
            printf("Trial %d: %.1f ms  %.2f GB/s  (sum=%llu)\n",
                   t, elapsed * 1000.0, bw_gbs, (unsigned long long)result);
    }

    free(arr);
    return 0;
}
