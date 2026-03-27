use std::env;
use std::process;
use std::sync::atomic::{AtomicI64, AtomicU8, Ordering};
use std::sync::{Arc, Barrier};
use std::thread;
use std::time::Instant;

// Worker task modes
const MODE_REGION: u8 = 0; // fork/join with empty body
const MODE_BARRIER: u8 = 1; // hit work_barrier N times
const MODE_ATOMIC: u8 = 2; // atomic increment N times
const MODE_SHUTDOWN: u8 = 255;

struct Config {
    threads: usize,
    region_reps: usize,
    barrier_reps: usize,
    atomic_reps: usize,
    trials: usize,
    csv: bool,
    warmup: bool,
}

struct BenchResult {
    region_time: f64,
    barrier_time: f64,
    atomic_time: f64,
    final_counter: i64,
}

// Thread pool that mirrors OpenMP's persistent thread team.
// Workers sleep on fork_barrier, execute their task, then sleep on join_barrier.
// Main thread drives the loop from outside.
struct Pool {
    handles: Vec<thread::JoinHandle<()>>,
    fork_barrier: Arc<Barrier>,
    join_barrier: Arc<Barrier>,
    work_barrier: Arc<Barrier>,
    mode: Arc<AtomicU8>,
    work_reps: Arc<AtomicI64>,
    counter: Arc<AtomicI64>,
}

impl Pool {
    fn new(threads: usize) -> Pool {
        let fork_barrier = Arc::new(Barrier::new(threads));
        let join_barrier = Arc::new(Barrier::new(threads));
        let work_barrier = Arc::new(Barrier::new(threads));
        let mode = Arc::new(AtomicU8::new(MODE_REGION));
        let work_reps = Arc::new(AtomicI64::new(0));
        let counter = Arc::new(AtomicI64::new(0));

        let mut handles = Vec::with_capacity(threads - 1);
        for _ in 0..threads - 1 {
            let fb = Arc::clone(&fork_barrier);
            let jb = Arc::clone(&join_barrier);
            let wb = Arc::clone(&work_barrier);
            let m = Arc::clone(&mode);
            let wr = Arc::clone(&work_reps);
            let c = Arc::clone(&counter);

            handles.push(thread::spawn(move || loop {
                fb.wait();
                let task = m.load(Ordering::Acquire);
                if task == MODE_SHUTDOWN {
                    break;
                }
                let reps = wr.load(Ordering::Acquire) as usize;
                match task {
                    MODE_BARRIER => {
                        for _ in 0..reps {
                            wb.wait();
                        }
                    }
                    MODE_ATOMIC => {
                        for _ in 0..reps {
                            c.fetch_add(1, Ordering::Relaxed);
                        }
                    }
                    _ => {} // MODE_REGION: empty body, mirrors OpenMP empty parallel region
                }
                jb.wait();
            }));
        }

        Pool {
            handles,
            fork_barrier,
            join_barrier,
            work_barrier,
            mode,
            work_reps,
            counter,
        }
    }

    // Mirrors: for (i=0; i<reps; i++) { #pragma omp parallel {} }
    // Measures the cost of fork/join N times with an empty region body.
    fn benchmark_region(&self, reps: usize) -> f64 {
        self.mode.store(MODE_REGION, Ordering::Release);
        self.work_reps.store(0, Ordering::Release);
        let start = Instant::now();
        for _ in 0..reps {
            self.fork_barrier.wait();
            self.join_barrier.wait();
        }
        start.elapsed().as_secs_f64()
    }

    // Mirrors: #pragma omp parallel { for (i=0; i<reps; i++) #pragma omp barrier }
    // Timing includes one region entry/exit (fork/join) plus N barrier hits.
    fn benchmark_barrier(&self, reps: usize) -> f64 {
        self.mode.store(MODE_BARRIER, Ordering::Release);
        self.work_reps.store(reps as i64, Ordering::Release);
        let start = Instant::now();
        self.fork_barrier.wait(); // enter region
        for _ in 0..reps {
            self.work_barrier.wait();
        }
        self.join_barrier.wait(); // exit region
        start.elapsed().as_secs_f64()
    }

    // Mirrors: #pragma omp parallel { for (i=0; i<reps; i++) #pragma omp atomic counter++ }
    fn benchmark_atomic(&self, reps: usize) -> f64 {
        self.counter.store(0, Ordering::Release);
        self.mode.store(MODE_ATOMIC, Ordering::Release);
        self.work_reps.store(reps as i64, Ordering::Release);
        let start = Instant::now();
        self.fork_barrier.wait(); // enter region
        for _ in 0..reps {
            self.counter.fetch_add(1, Ordering::Relaxed);
        }
        self.join_barrier.wait(); // exit region
        start.elapsed().as_secs_f64()
    }

    fn final_counter(&self) -> i64 {
        self.counter.load(Ordering::SeqCst)
    }

    fn shutdown(self) {
        self.mode.store(MODE_SHUTDOWN, Ordering::Release);
        self.fork_barrier.wait();
        for h in self.handles {
            h.join().unwrap();
        }
    }
}

fn run_one_trial(pool: &Pool, cfg: &Config) -> BenchResult {
    let region_time = pool.benchmark_region(cfg.region_reps);
    let barrier_time = pool.benchmark_barrier(cfg.barrier_reps);
    let atomic_time = pool.benchmark_atomic(cfg.atomic_reps);
    let final_counter = pool.final_counter();
    BenchResult { region_time, barrier_time, atomic_time, final_counter }
}

fn print_csv_header() {
    println!(
        "trial,threads,region_reps,barrier_reps,atomic_reps,\
         region_total_sec,region_avg_sec,\
         barrier_total_sec,barrier_avg_sec,\
         atomic_total_sec,atomic_avg_sec_per_increment,\
         final_counter,expected_counter,correct"
    );
}

fn print_csv_row(cfg: &Config, trial: usize, r: &BenchResult) {
    let expected = (cfg.threads * cfg.atomic_reps) as i64;
    let region_avg = r.region_time / cfg.region_reps as f64;
    let barrier_avg = r.barrier_time / cfg.barrier_reps as f64;
    let atomic_avg = r.atomic_time / expected as f64;
    let correct = if r.final_counter == expected { 1 } else { 0 };

    println!(
        "{},{},{},{},{},{:.9},{:.9},{:.9},{:.9},{:.9},{:.9},{},{},{}",
        trial,
        cfg.threads,
        cfg.region_reps,
        cfg.barrier_reps,
        cfg.atomic_reps,
        r.region_time,
        region_avg,
        r.barrier_time,
        barrier_avg,
        r.atomic_time,
        atomic_avg,
        r.final_counter,
        expected,
        correct,
    );
}

fn print_human_readable(cfg: &Config, trial: usize, r: &BenchResult) {
    let expected = (cfg.threads * cfg.atomic_reps) as i64;
    println!("=== Rust Overhead Benchmark ===");
    println!("trial         : {}", trial);
    println!("threads       : {}", cfg.threads);
    println!("region_reps   : {}", cfg.region_reps);
    println!("barrier_reps  : {}", cfg.barrier_reps);
    println!("atomic_reps   : {}", cfg.atomic_reps);
    println!();
    println!("[Parallel Region]");
    println!("total_time_sec          : {:.9}", r.region_time);
    println!("avg_region_time_sec     : {:.9}", r.region_time / cfg.region_reps as f64);
    println!();
    println!("[Barrier]");
    println!("total_time_sec          : {:.9}", r.barrier_time);
    println!("avg_barrier_time_sec    : {:.9}", r.barrier_time / cfg.barrier_reps as f64);
    println!();
    println!("[Atomic Increment]");
    println!("total_time_sec          : {:.9}", r.atomic_time);
    println!("avg_atomic_time_sec     : {:.9}", r.atomic_time / expected as f64);
    println!("final_counter           : {}", r.final_counter);
    println!("expected_counter        : {}", expected);
    println!(
        "correct                 : {}",
        if r.final_counter == expected { "yes" } else { "no" }
    );
    println!();
}

fn print_usage(prog: &str) {
    eprintln!(
        "Usage: {} [threads] [region_reps] [barrier_reps] [atomic_reps] [trials] [--csv] [--warmup]",
        prog
    );
    eprintln!("Example:");
    eprintln!("  {} 8 100000 100000 100000 5", prog);
    eprintln!("  {} 8 100000 100000 100000 5 --csv", prog);
    eprintln!("  {} 8 100000 100000 100000 5 --csv --warmup", prog);
}

fn parse_args() -> Config {
    let args: Vec<String> = env::args().collect();
    let prog = &args[0];

    let mut cfg = Config {
        threads: 4,
        region_reps: 100000,
        barrier_reps: 100000,
        atomic_reps: 100000,
        trials: 5,
        csv: false,
        warmup: false,
    };

    let mut numeric_count = 0;
    for arg in args.iter().skip(1) {
        match arg.as_str() {
            "--csv" => cfg.csv = true,
            "--warmup" => cfg.warmup = true,
            _ => {
                let v: usize = match arg.parse() {
                    Ok(n) => n,
                    Err(_) => {
                        print_usage(prog);
                        process::exit(1);
                    }
                };
                numeric_count += 1;
                match numeric_count {
                    1 => cfg.threads = v,
                    2 => cfg.region_reps = v,
                    3 => cfg.barrier_reps = v,
                    4 => cfg.atomic_reps = v,
                    5 => cfg.trials = v,
                    _ => {
                        print_usage(prog);
                        process::exit(1);
                    }
                }
            }
        }
    }

    if cfg.threads == 0
        || cfg.region_reps == 0
        || cfg.barrier_reps == 0
        || cfg.atomic_reps == 0
        || cfg.trials == 0
    {
        print_usage(prog);
        process::exit(1);
    }

    cfg
}

fn main() {
    let cfg = parse_args();
    let pool = Pool::new(cfg.threads);

    if cfg.warmup {
        run_one_trial(&pool, &cfg);
    }

    if cfg.csv {
        print_csv_header();
    }

    for trial in 1..=cfg.trials {
        let r = run_one_trial(&pool, &cfg);
        if cfg.csv {
            print_csv_row(&cfg, trial, &r);
        } else {
            print_human_readable(&cfg, trial, &r);
        }
    }

    pool.shutdown();
}
