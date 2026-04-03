// Benchmark 3: Parallel Histogram — Rust Strategy A (private + merge)
//
// Each thread builds its own private histogram (plain Vec<u64>) during compute.
// No atomics, no shared state during the parallel phase — zero contention.
// After all threads finish, the main thread merges all private histograms
// into the final result explicitly.
//
// This is the Rust equivalent of OpenMP's array reduction clause.
// The contrast with the OpenMP version is the key Q5A finding:
//
//   OpenMP — one clause handles everything invisibly:
//     #pragma omp parallel for reduction(+: h[:bins])
//
//   Rust — the programmer must write every step explicitly:
//     1. Allocate a private Vec<u64> per thread
//     2. Each thread fills its own copy (no synchronization needed)
//     3. Return the private histogram from the thread
//     4. Join all threads and merge in an explicit loop
//
// Both achieve the same result and scale equally well.
// The difference is entirely in how much the programmer must express.
//
// Usage:
//   cargo run --release -- [threads] [n] [num_bins] [trials] [--csv] [--warmup]
//
// Examples:
//   cargo run --release -- 8 67108864 256 5 --csv --warmup
//   cargo run --release -- 1 67108864 256 5 --csv

use std::sync::Arc;
use std::thread;
use std::time::Instant;

const DEFAULT_N:        u64   = 1 << 26;  // 67,108,864 elements
const DEFAULT_THREADS:  usize = 1;
const DEFAULT_BINS:     usize = 256;
const DEFAULT_TRIALS:   usize = 5;
const WARMUP_N:         u64   = 1 << 20;

struct Config {
    threads:  usize,
    n:        u64,
    num_bins: usize,
    trials:   usize,
    csv:      bool,
    warmup:   bool,
}

fn parse_args() -> Config {
    let args: Vec<String> = std::env::args().collect();
    let mut cfg = Config {
        threads:  DEFAULT_THREADS,
        n:        DEFAULT_N,
        num_bins: DEFAULT_BINS,
        trials:   DEFAULT_TRIALS,
        csv:      false,
        warmup:   false,
    };
    let mut pos = 0usize;
    for arg in &args[1..] {
        match arg.as_str() {
            "--csv"    => cfg.csv    = true,
            "--warmup" => cfg.warmup = true,
            other => {
                match pos {
                    0 => cfg.threads  = other.parse().expect("threads must be a positive integer"),
                    1 => cfg.n        = other.parse().expect("n must be a positive integer"),
                    2 => cfg.num_bins = other.parse().expect("num_bins must be a positive integer"),
                    3 => cfg.trials   = other.parse().expect("trials must be a positive integer"),
                    _ => {}
                }
                pos += 1;
            }
        }
    }
    cfg
}

// Generate random input data using Xorshift64 (lower 32 bits).
// Identical seed and shifts as the C++ version.
// Generated once before all trials — not part of the timed region.
fn generate_data(n: u64) -> Vec<u32> {
    let mut data = Vec::with_capacity(n as usize);
    let mut state: u64 = 0xdeadbeefcafe1234;
    for _ in 0..n {
        state ^= state << 13;
        state ^= state >>  7;
        state ^= state << 17;
        data.push(state as u32);
    }
    data
}

// Run one histogram trial using Strategy A.
//
// Step 1 — Parallel phase (no synchronization):
//   Each thread receives a static range of data[start..end].
//   It allocates a private Vec<u64> of size num_bins, all zeros.
//   It iterates its range and increments its own private histogram.
//   It returns the private histogram when done.
//
// Step 2 — Merge phase (single-threaded):
//   The main thread joins all handles and adds each private histogram
//   into the final result bin by bin.
//
// Compare with OpenMP: reduction(+: h[:bins]) does both steps invisibly.
fn run_trial(cfg: &Config, data: &Arc<Vec<u32>>) -> Vec<u64> {
    let n        = cfg.n;
    let bins     = cfg.num_bins;
    let threads  = cfg.threads;
    let chunk    = n / threads as u64;
    let remainder = n % threads as u64;

    // --- Step 1: spawn threads, each with a private histogram ---
    let handles: Vec<_> = (0..threads)
        .map(|tid| {
            let data = Arc::clone(data);

            // Static partitioning — matches OpenMP schedule(static)
            let start = tid as u64 * chunk;
            let end   = if tid == threads - 1 {
                start + chunk + remainder
            } else {
                start + chunk
            };

            thread::spawn(move || {
                // Private histogram — no sharing, no atomics needed
                let mut local_hist = vec![0u64; bins];

                for i in start..end {
                    let bucket = (data[i as usize] % bins as u32) as usize;
                    local_hist[bucket] += 1;  // plain increment, no synchronization
                }

                local_hist  // return private histogram to main thread
            })
        })
        .collect();

    // --- Step 2: join threads and merge all private histograms ---
    let mut hist = vec![0u64; bins];
    for handle in handles {
        let local_hist = handle.join().unwrap();
        for (b, &count) in local_hist.iter().enumerate() {
            hist[b] += count;  // explicit merge — OpenMP's reduction clause does this invisibly
        }
    }

    hist
}

fn main() {
    let cfg = parse_args();

    // Generate input data once — outside all timing
    let data: Arc<Vec<u32>> = Arc::new(generate_data(cfg.n));

    if cfg.warmup {
        let warmup_data: Arc<Vec<u32>> = Arc::new(
            data[..WARMUP_N as usize].to_vec()
        );
        let warmup_cfg = Config {
            threads: cfg.threads, n: WARMUP_N, num_bins: cfg.num_bins,
            trials: 1, csv: false, warmup: false,
        };
        run_trial(&warmup_cfg, &warmup_data);
    }

    if cfg.csv {
        println!("trial,threads,n,num_bins,elapsed_sec,correct");
    }

    for trial in 1..=cfg.trials {
        let start = Instant::now();
        let hist = run_trial(&cfg, &data);
        let elapsed = start.elapsed().as_secs_f64();

        // Verify: sum of all bins must equal N
        let total: u64 = hist.iter().sum();
        let correct = u8::from(total == cfg.n);

        if cfg.csv {
            println!(
                "{},{},{},{},{:.6},{}",
                trial, cfg.threads, cfg.n, cfg.num_bins, elapsed, correct
            );
        } else {
            println!(
                "Trial {:2}: {:.6}s  bins={}  correct={}",
                trial, elapsed, cfg.num_bins, correct
            );
        }
    }
}
