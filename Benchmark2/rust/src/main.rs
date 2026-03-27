use std::env;
use std::process;
use std::thread;
use std::time::Instant;

// Monte Carlo Pi estimation.
//
// Each thread independently throws darts at the unit square [0,1) x [0,1).
// A dart is a "hit" if x^2 + y^2 < 1.0 (inside the unit circle).
// pi ~= 4 * hits / total_samples
//
// Parallelism structure:
//   - total samples are divided evenly across threads
//   - each thread runs fully independently with its own RNG
//   - main thread sums hit counts after all threads finish (join)
//   - no shared state during sampling — one reduction at the end
//
// This directly mirrors the OpenMP version's structure:
//   #pragma omp parallel reduction(+:hits)
//   #pragma omp for schedule(static)
//
// RNG: xorshift64 — fast, no external crates needed, sufficient quality
// for Monte Carlo estimation. Seeded per thread with (42 + thread_id)
// to match the OpenMP version's seeding strategy.

struct Config {
    threads: usize,
    samples: u64,
    trials:  usize,
    csv:     bool,
    warmup:  bool,
}

struct BenchResult {
    pi_estimate: f64,
    elapsed_sec: f64,
}

// Xorshift64 RNG — equivalent role to mt19937_64 in the C++ version.
// No external dependencies required.
struct Xorshift64 {
    state: u64,
}

impl Xorshift64 {
    fn new(seed: u64) -> Self {
        // xorshift64 must not have a zero state
        Self { state: if seed == 0 { 0xdeadbeef } else { seed } }
    }

    #[inline]
    fn next_u64(&mut self) -> u64 {
        let mut x = self.state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        self.state = x;
        x
    }

    // Returns a uniform f64 in [0.0, 1.0)
    #[inline]
    fn next_f64(&mut self) -> f64 {
        // Use upper 53 bits to fill the mantissa of an IEEE 754 double
        (self.next_u64() >> 11) as f64 * (1.0_f64 / (1u64 << 53) as f64)
    }
}

fn run_one_trial(cfg: &Config) -> BenchResult {
    let samples_per_thread = cfg.samples / cfg.threads as u64;
    let remainder          = cfg.samples % cfg.threads as u64;

    let start = Instant::now();

    let handles: Vec<_> = (0..cfg.threads)
        .map(|tid| {
            // Last thread takes any leftover samples so the total is exact
            let my_samples = if tid == cfg.threads - 1 {
                samples_per_thread + remainder
            } else {
                samples_per_thread
            };

            // Seed mirrors OpenMP version: 42 + thread_id
            let seed = 42 + tid as u64;

            thread::spawn(move || {
                let mut rng  = Xorshift64::new(seed);
                let mut hits = 0u64;
                for _ in 0..my_samples {
                    let x = rng.next_f64();
                    let y = rng.next_f64();
                    if x * x + y * y < 1.0 {
                        hits += 1;
                    }
                }
                hits
            })
        })
        .collect();

    // Join all threads and sum hit counts — equivalent to OpenMP reduction
    let total_hits: u64 = handles.into_iter().map(|h| h.join().unwrap()).sum();

    let elapsed = start.elapsed().as_secs_f64();

    BenchResult {
        pi_estimate: 4.0 * total_hits as f64 / cfg.samples as f64,
        elapsed_sec: elapsed,
    }
}

fn print_csv_header() {
    println!("trial,threads,samples,elapsed_sec,pi_estimate,pi_error");
}

fn print_csv_row(cfg: &Config, trial: usize, r: &BenchResult) {
    let error = r.pi_estimate - std::f64::consts::PI;
    println!(
        "{},{},{},{:.9},{:.9},{:.9}",
        trial, cfg.threads, cfg.samples, r.elapsed_sec, r.pi_estimate, error
    );
}

fn print_human_readable(cfg: &Config, trial: usize, r: &BenchResult) {
    let error = r.pi_estimate - std::f64::consts::PI;
    println!("=== Rust Monte Carlo Pi Benchmark ===");
    println!("trial         : {}", trial);
    println!("threads       : {}", cfg.threads);
    println!("samples       : {}", cfg.samples);
    println!();
    println!("elapsed_sec   : {:.9}", r.elapsed_sec);
    println!("pi_estimate   : {:.9}", r.pi_estimate);
    println!("pi_error      : {:.9}", error);
    println!();
}

fn print_usage(prog: &str) {
    eprintln!(
        "Usage: {} [threads] [samples] [trials] [--csv] [--warmup]",
        prog
    );
    eprintln!("Example:");
    eprintln!("  {} 4 100000000 5", prog);
    eprintln!("  {} 4 100000000 5 --csv", prog);
    eprintln!("  {} 4 100000000 5 --csv --warmup", prog);
}

fn parse_args() -> Config {
    let args: Vec<String> = env::args().collect();
    let prog = &args[0];

    let mut cfg = Config {
        threads: 4,
        samples: 100_000_000,
        trials:  5,
        csv:     false,
        warmup:  false,
    };

    let mut numeric_count = 0;
    for arg in args.iter().skip(1) {
        match arg.as_str() {
            "--csv"    => cfg.csv    = true,
            "--warmup" => cfg.warmup = true,
            _ => {
                let v: u64 = match arg.parse() {
                    Ok(n)  => n,
                    Err(_) => { print_usage(prog); process::exit(1); }
                };
                numeric_count += 1;
                match numeric_count {
                    1 => cfg.threads = v as usize,
                    2 => cfg.samples = v,
                    3 => cfg.trials  = v as usize,
                    _ => { print_usage(prog); process::exit(1); }
                }
            }
        }
    }

    if cfg.threads == 0 || cfg.samples == 0 || cfg.trials == 0 {
        print_usage(prog);
        process::exit(1);
    }

    cfg
}

fn main() {
    let cfg = parse_args();

    if cfg.warmup {
        let warmup_cfg = Config {
            threads: cfg.threads,
            samples: 1_000_000,
            trials:  1,
            csv:     false,
            warmup:  false,
        };
        run_one_trial(&warmup_cfg);
    }

    if cfg.csv {
        print_csv_header();
    }

    for trial in 1..=cfg.trials {
        let r = run_one_trial(&cfg);
        if cfg.csv {
            print_csv_row(&cfg, trial, &r);
        } else {
            print_human_readable(&cfg, trial, &r);
        }
    }
}
