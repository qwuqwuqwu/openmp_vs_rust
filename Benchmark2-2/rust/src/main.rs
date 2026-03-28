// Benchmark 2-2: Popcount Sum using Rayon
//
// Identical workload to Benchmark 2-1: sum popcount(i) for i in 0..N.
// Key difference: parallelism is handled by a persistent Rayon thread pool
// rather than bare std::thread::spawn/join.
//
// Usage:
//   cargo run --release -- [threads] [n] [trials] [--csv] [--warmup]
//
// Examples:
//   cargo run --release -- 8 8589934592 5 --csv --warmup
//   cargo run --release -- 1 8589934592 5 --csv

use rayon::prelude::*;
use std::time::Instant;

const DEFAULT_N: u64 = 1 << 33;       // 8,589,934,592
const DEFAULT_THREADS: usize = 1;
const DEFAULT_TRIALS: usize = 5;
const EXPECTED_BITS: u64 = 141_733_920_768; // 33 × 2^32
const WARMUP_N: u64 = 1 << 20;

struct Config {
    threads: usize,
    n: u64,
    trials: usize,
    csv: bool,
    warmup: bool,
}

fn parse_args() -> Config {
    let args: Vec<String> = std::env::args().collect();
    let mut cfg = Config {
        threads: DEFAULT_THREADS,
        n: DEFAULT_N,
        trials: DEFAULT_TRIALS,
        csv: false,
        warmup: false,
    };
    let mut pos = 0usize;
    for arg in &args[1..] {
        match arg.as_str() {
            "--csv"    => cfg.csv = true,
            "--warmup" => cfg.warmup = true,
            other => {
                match pos {
                    0 => cfg.threads = other.parse().expect("threads must be a positive integer"),
                    1 => cfg.n       = other.parse().expect("n must be a positive integer"),
                    2 => cfg.trials  = other.parse().expect("trials must be a positive integer"),
                    _ => {}
                }
                pos += 1;
            }
        }
    }
    cfg
}

/// Run one popcount trial inside the given Rayon thread pool.
/// The pool is persistent — it is built once and reused across all trials,
/// matching OpenMP's persistent thread pool behaviour.
fn popcount_sum(pool: &rayon::ThreadPool, n: u64) -> u64 {
    pool.install(|| {
        (0u64..n)
            .into_par_iter()
            .map(|i| i.count_ones() as u64)
            .sum()
    })
}

fn main() {
    let cfg = parse_args();

    // Build the Rayon thread pool exactly once.
    // All trials reuse the same pool — threads are never destroyed between trials.
    // This is the direct equivalent of OpenMP's persistent thread pool.
    let pool = rayon::ThreadPoolBuilder::new()
        .num_threads(cfg.threads)
        .build()
        .expect("Failed to build Rayon thread pool");

    // Optional warmup: one small run to let the pool warm up and the OS
    // finish scheduling all threads before timing begins.
    if cfg.warmup {
        let _ = popcount_sum(&pool, WARMUP_N);
    }

    if cfg.csv {
        println!("trial,threads,n,elapsed_sec,total_bits,expected_bits,correct");
    }

    for trial in 1..=cfg.trials {
        let start = Instant::now();
        let total_bits = popcount_sum(&pool, cfg.n);
        let elapsed = start.elapsed().as_secs_f64();

        let correct = u8::from(total_bits == EXPECTED_BITS);

        if cfg.csv {
            println!(
                "{},{},{},{:.12},{},{},{}",
                trial, cfg.threads, cfg.n, elapsed,
                total_bits, EXPECTED_BITS, correct
            );
        } else {
            println!(
                "Trial {:2}: {:.6}s  bits={}  expected={}  correct={}",
                trial, elapsed, total_bits, EXPECTED_BITS, correct
            );
        }
    }
}
