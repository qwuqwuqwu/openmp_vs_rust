// Benchmark 4: Irregular Workload — Prime Testing (Rust)
//
// Count all primes in [2, N] using trial division.
//
// Why this workload is irregular:
//   Checking whether n is prime requires trial division up to √n.
//   - Small composites (e.g. n=4): 1 division, done instantly.
//   - Large primes (e.g. n=999,983): ~1000 divisions before confirming prime.
//   Cost per element varies by orders of magnitude across the range.
//   Large primes are denser near N, so the last static chunk always gets
//   the heaviest work — static partitioning bottlenecks on that thread.
//
// Three scheduling strategies implemented in this binary:
//
//   static  — std::thread with manual fixed-range partitioning.
//             Thread tid owns [tid*chunk, (tid+1)*chunk).
//             Identical load imbalance to OpenMP schedule(static).
//             Zero coordination overhead during compute.
//
//   dynamic — std::thread with shared Arc<AtomicU64> counter.
//             Each thread calls fetch_add(chunk_size) to claim next chunk.
//             Idle threads pick up new work automatically — matches
//             OpenMP schedule(dynamic, chunk_size) semantics.
//
//   rayon   — Rayon par_iter with work-stealing.
//             (2u64..=n).into_par_iter().filter(|&i| is_prime(i)).count()
//             Work-stealing: each thread has a local deque; idle threads
//             steal half of a busy thread's remaining range automatically.
//             No programmer-visible coordination code.
//
// Usage:
//   ./rust_benchmark4 [threads] [n] [schedule] [chunk_size] [trials] [--csv] [--warmup]
//   schedule: "static", "dynamic", or "rayon"
//
// Examples:
//   ./rust_benchmark4 8 1000000 static  0   5 --csv --warmup
//   ./rust_benchmark4 8 1000000 dynamic 100 5 --csv --warmup
//   ./rust_benchmark4 8 1000000 rayon   0   5 --csv --warmup

use std::env;
use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use std::thread;
use std::time::Instant;
use rayon::prelude::*;

const DEFAULT_N:          u64   = 1_000_000;
const DEFAULT_THREADS:    usize = 1;
const DEFAULT_CHUNK_SIZE: u64   = 100;
const DEFAULT_TRIALS:     usize = 5;
const EXPECTED_PRIMES:    u64   = 78_498;

#[derive(Clone)]
enum Schedule {
    Static,
    Dynamic,
    Rayon,
}

#[derive(Clone)]
struct Config {
    threads:    usize,
    n:          u64,
    schedule:   Schedule,
    chunk_size: u64,
    trials:     usize,
    csv:        bool,
    warmup:     bool,
}

fn parse_args() -> Config {
    let args: Vec<String> = env::args().collect();
    let mut cfg = Config {
        threads:    DEFAULT_THREADS,
        n:          DEFAULT_N,
        schedule:   Schedule::Static,
        chunk_size: DEFAULT_CHUNK_SIZE,
        trials:     DEFAULT_TRIALS,
        csv:        false,
        warmup:     false,
    };
    let mut pos = 0usize;
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--csv"    => { cfg.csv    = true; }
            "--warmup" => { cfg.warmup = true; }
            s => {
                match pos {
                    0 => cfg.threads    = s.parse().expect("threads"),
                    1 => cfg.n          = s.parse().expect("n"),
                    2 => cfg.schedule   = match s {
                        "dynamic" => Schedule::Dynamic,
                        "rayon"   => Schedule::Rayon,
                        _         => Schedule::Static,
                    },
                    3 => cfg.chunk_size = s.parse().expect("chunk_size"),
                    4 => cfg.trials     = s.parse().expect("trials"),
                    _ => {}
                }
                pos += 1;
            }
        }
        i += 1;
    }
    cfg
}

// Trial division primality test.
// Returns true if n is prime.
// Cost: O(√n) in the worst case (n is prime).
// This non-uniform cost is the source of load imbalance.
#[inline]
fn is_prime(n: u64) -> bool {
    if n < 2      { return false; }
    if n == 2     { return true;  }
    if n % 2 == 0 { return false; }
    let mut i = 3u64;
    while i * i <= n {
        if n % i == 0 { return false; }
        i += 2;
    }
    true
}

// Static scheduling: thread tid owns a fixed contiguous range.
// Exactly mirrors OpenMP schedule(static) with default chunk size.
// Load imbalance is identical: the last thread owns the highest range,
// which contains the densest and most expensive primes.
fn run_static(cfg: &Config) -> u64 {
    let n         = cfg.n;
    let nthreads  = cfg.threads;
    let chunk     = (n - 1) / nthreads as u64;  // range is [2, n], size = n-1
    let remainder = (n - 1) % nthreads as u64;

    let handles: Vec<_> = (0..nthreads).map(|tid| {
        // Each thread gets a fixed contiguous range.
        // Last thread absorbs any remainder so we cover [2, n] exactly.
        let start = 2 + tid as u64 * chunk;
        let end   = if tid == nthreads - 1 {
            start + chunk + remainder
        } else {
            start + chunk - 1
        };

        thread::spawn(move || {
            let mut count = 0u64;
            for i in start..=end {
                if is_prime(i) { count += 1; }
            }
            count
        })
    }).collect();

    handles.into_iter().map(|h| h.join().unwrap()).sum()
}

// Dynamic scheduling: threads share an atomic counter.
// Each thread calls fetch_add(chunk_size) to claim the next chunk of work.
// Idle threads immediately pick up more work — load balances automatically.
// Mirrors OpenMP schedule(dynamic, chunk_size) semantics.
//
// The atomic fetch_add is the Rust equivalent of OpenMP's internal work queue.
// One fetch_add per chunk (not per element), so overhead is amortized.
fn run_dynamic(cfg: &Config) -> u64 {
    let n          = cfg.n;
    let nthreads   = cfg.threads;
    let chunk_size = cfg.chunk_size;

    // Shared counter: next available work item.
    // All threads read/increment this atomically.
    let next = Arc::new(AtomicU64::new(2));

    let handles: Vec<_> = (0..nthreads).map(|_| {
        let next  = Arc::clone(&next);
        thread::spawn(move || {
            let mut count = 0u64;
            loop {
                // Claim the next chunk atomically.
                let start = next.fetch_add(chunk_size, Ordering::Relaxed);
                if start > n { break; }
                let end = (start + chunk_size - 1).min(n);
                for i in start..=end {
                    if is_prime(i) { count += 1; }
                }
            }
            count
        })
    }).collect();

    handles.into_iter().map(|h| h.join().unwrap()).sum()
}

// Rayon scheduling: work-stealing via par_iter.
// Rayon splits the range recursively into a deque per thread.
// When a thread finishes its local work, it steals half of a busy thread's
// remaining deque — automatic load balancing with no programmer intervention.
//
// The thread count is set via a global Rayon thread pool built here.
// This is the Rayon equivalent of setting OMP_NUM_THREADS.
fn run_rayon(cfg: &Config) -> u64 {
    let pool = rayon::ThreadPoolBuilder::new()
        .num_threads(cfg.threads)
        .build()
        .unwrap();

    pool.install(|| {
        (2u64..=cfg.n)
            .into_par_iter()
            .filter(|&i| is_prime(i))
            .count() as u64
    })
}

fn run_trial(cfg: &Config) -> u64 {
    match cfg.schedule {
        Schedule::Static  => run_static(cfg),
        Schedule::Dynamic => run_dynamic(cfg),
        Schedule::Rayon   => run_rayon(cfg),
    }
}

fn sched_name(cfg: &Config) -> &'static str {
    match cfg.schedule {
        Schedule::Static  => "static",
        Schedule::Dynamic => "dynamic",
        Schedule::Rayon   => "rayon",
    }
}

fn main() {
    let cfg = parse_args();
    let name = sched_name(&cfg);

    if cfg.warmup {
        let mut warm = cfg.clone();
        warm.n = 10_000;
        run_trial(&warm);
    }

    if cfg.csv {
        println!("trial,threads,n,schedule,chunk_size,elapsed_sec,prime_count,correct");
    }

    for t in 1..=cfg.trials {
        let start = Instant::now();
        let prime_count = run_trial(&cfg);
        let elapsed = start.elapsed().as_secs_f64();

        let correct = if prime_count == EXPECTED_PRIMES { 1 } else { 0 };

        if cfg.csv {
            let chunk_col = match cfg.schedule {
                Schedule::Dynamic => cfg.chunk_size,
                _                 => 0,
            };
            println!("{},{},{},{},{},{:.6},{},{}",
                t, cfg.threads, cfg.n, name, chunk_col,
                elapsed, prime_count, correct);
        } else {
            println!("Trial {}: {:.6}s  schedule={}  primes={}  correct={}",
                t, elapsed, name, prime_count, correct);
        }
    }
}
