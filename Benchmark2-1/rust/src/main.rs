use std::env;
use std::process;
use std::thread;
use std::time::Instant;

// Popcount Sum Benchmark.
//
// Computes the total number of set bits across all integers from 0 to N-1:
//
//   total = Σᵢ₌₀ᴺ⁻¹ popcount(i)
//
// Uses i.count_ones() which the Rust compiler (LLVM) maps directly to the
// single-cycle hardware `popcnt` instruction on x86 — identical in role to
// C++'s __builtin_popcountll. Both emit the same machine instruction, so
// any performance difference between this and the OpenMP version comes
// purely from the parallelism model and compiler loop optimization strategy.
//
// Parallelism structure:
//   - Range [0, N) is divided into equal contiguous chunks across threads
//     (static partitioning — mirrors #pragma omp for schedule(static))
//   - Each thread accumulates a private u64 local sum with no shared writes
//   - Main thread collects results via join() and sums with fold
//   - No mutexes, no atomics, no channels during the computation phase
//   - One reduction at the very end (join + fold)
//
// Why count_ones() was chosen over a crate:
//   count_ones() is a stable method on all integer types in Rust's standard
//   library. It compiles to a single `popcnt` instruction under --release.
//   No external dependencies are needed, keeping the comparison at the
//   language + std level, consistent with the rest of this project.
//
// Correctness verification:
//   For N = 2^k: expected = k × 2^(k-1)
//   For N = 2^33 = 8,589,934,592: expected = 33 × 2^32 = 141,733,920,768

const DEFAULT_N: u64    = 1 << 33;              // 8,589,934,592
const EXPECTED_BITS: u64 = 141_733_920_768;     // 33 * 2^32

struct Config {
    threads: usize,
    n:       u64,
    trials:  usize,
    csv:     bool,
    warmup:  bool,
}

struct BenchResult {
    total_bits:  u64,
    elapsed_sec: f64,
}

fn run_one_trial(cfg: &Config) -> BenchResult {
    let chunk     = cfg.n / cfg.threads as u64;
    let remainder = cfg.n % cfg.threads as u64;

    let start = Instant::now();

    // Spawn one thread per slot. Static partitioning: thread `tid` owns
    // the contiguous range [tid*chunk, (tid+1)*chunk), with the last thread
    // absorbing any remainder so the total is always exactly N.
    let handles: Vec<_> = (0..cfg.threads)
        .map(|tid| {
            let start_i = tid as u64 * chunk;
            let end_i   = if tid == cfg.threads - 1 {
                start_i + chunk + remainder
            } else {
                start_i + chunk
            };

            thread::spawn(move || {
                let mut local: u64 = 0;
                for i in start_i..end_i {
                    local += i.count_ones() as u64;
                }
                local
            })
        })
        .collect();

    // Join all threads and sum their partial results.
    // Equivalent to OpenMP's reduction(+:total).
    let total_bits: u64 = handles
        .into_iter()
        .map(|h| h.join().unwrap())
        .sum();

    let elapsed = start.elapsed().as_secs_f64();

    BenchResult { total_bits, elapsed_sec: elapsed }
}

fn print_csv_header() {
    println!("trial,threads,n,elapsed_sec,total_bits,expected_bits,correct");
}

fn print_csv_row(cfg: &Config, trial: usize, r: &BenchResult) {
    let expected = if cfg.n == DEFAULT_N { EXPECTED_BITS } else { 0 };
    let correct  = if expected == 0 {
        -1i32
    } else if r.total_bits == expected {
        1
    } else {
        0
    };
    println!(
        "{},{},{},{:.12},{},{},{}",
        trial, cfg.threads, cfg.n,
        r.elapsed_sec, r.total_bits, expected, correct
    );
}

fn print_human_readable(cfg: &Config, trial: usize, r: &BenchResult) {
    let expected = if cfg.n == DEFAULT_N { EXPECTED_BITS } else { 0 };
    let correct  = expected == 0 || r.total_bits == expected;
    println!("=== Rust Popcount Sum Benchmark ===");
    println!("trial         : {}", trial);
    println!("threads       : {}", cfg.threads);
    println!("n             : {}", cfg.n);
    println!();
    println!("elapsed_sec   : {:.12}", r.elapsed_sec);
    println!("total_bits    : {}", r.total_bits);
    if expected > 0 {
        println!("expected_bits : {}", expected);
        println!("correct       : {}", if correct { "YES" } else { "NO" });
    }
    println!();
}

fn print_usage(prog: &str) {
    eprintln!(
        "Usage: {} [threads] [n] [trials] [--csv] [--warmup]",
        prog
    );
    eprintln!("  n : total integers to process (default 2^33 = 8589934592)");
    eprintln!("Example:");
    eprintln!("  {} 4 8589934592 5 --csv --warmup", prog);
}

fn parse_args() -> Config {
    let args: Vec<String> = env::args().collect();
    let prog = &args[0];

    let mut cfg = Config {
        threads: 4,
        n:       DEFAULT_N,
        trials:  5,
        csv:     false,
        warmup:  false,
    };

    let mut numeric_count = 0usize;
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
                    2 => cfg.n       = v,
                    3 => cfg.trials  = v as usize,
                    _ => { print_usage(prog); process::exit(1); }
                }
            }
        }
    }

    if cfg.threads == 0 || cfg.n == 0 || cfg.trials == 0 {
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
            n:       1 << 20,   // 1M items — enough to warm thread pool
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
