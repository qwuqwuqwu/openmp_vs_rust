/*
 * Benchmark 5: Thread-to-Core Affinity (Rust)
 * Workload: parallel sum of a large u64 array (memory-bandwidth-bound)
 *
 * Affinity strategies (selected via argv[3]):
 *   "default" — no pinning; OS decides thread placement
 *   "spread"  — pin each thread to a core spread across all NUMA nodes
 *                 core_id = available_cores[tid * stride]
 *                 stride  = total_cores / num_threads
 *   "close"   — pin threads to consecutive cores (pack into fewest nodes)
 *                 core_id = available_cores[tid]
 *
 * FIRST-TOUCH POLICY:
 *   The array is initialized in parallel with the same pinning strategy as
 *   the compute phase. Linux first-touch policy places each page on the NUMA
 *   node of the thread that writes it first. Without this, vec![0; n] would
 *   zero-initialize on the main thread, landing all pages on NUMA node 0.
 *   We avoid this by allocating uninitialized capacity (unsafe) and writing
 *   in parallel.
 *
 * LOC for affinity control (Rust):
 *   ─ Cargo.toml: core_affinity = "0.8"            (+1 dependency)
 *   ─ build_pin_list():  ~8 lines                   (strategy → core list)
 *   ─ set_for_current(): 1 line per thread spawn    (actually pins the thread)
 *   Total: ~15 lines, 1 external crate
 *   Compare to OpenMP: 1 keyword in the pragma, 0 new dependencies.
 *
 * Usage:
 *   ./rust_benchmark5 [threads] [n] [strategy] [trials] [--csv] [--warmup]
 *   e.g.: ./rust_benchmark5 32 134217728 spread 5 --csv --warmup
 */

use core_affinity::CoreId;
use std::sync::Arc;
use std::thread;
use std::time::Instant;

/// Default: 128 M × 8 bytes = 1 GB
const N_DEFAULT: usize = 128 * 1024 * 1024;

/* ── Affinity helpers ────────────────────────────────────────────────────── */

/// Build the list of cores to pin each thread to.
/// Returns None for threads that should not be pinned (strategy = "default").
///
/// spread: stride = total_cores / num_threads → one core per NUMA group
/// close:  cores 0, 1, 2, … num_threads-1    → pack into fewest nodes
///
/// This function plus set_for_current() below constitute the entire
/// pinning implementation (~15 lines total).
fn build_pin_list(
    strategy: &str,
    num_threads: usize,
    core_ids: &[CoreId],
) -> Vec<Option<CoreId>> {
    (0..num_threads)
        .map(|tid| match strategy {
            "spread" => {
                let stride = (core_ids.len() / num_threads).max(1);
                core_ids.get(tid * stride).copied()
            }
            "close" => core_ids.get(tid).copied(),
            _ => None, // "default": do not pin
        })
        .collect()
}

/* ── First-touch parallel initialization ────────────────────────────────── */

fn parallel_init(arr: &mut Vec<u64>, num_threads: usize, strategy: &str, core_ids: &[CoreId]) {
    let n = arr.capacity();
    // Safety: capacity() was reserved with Vec::with_capacity(n).
    // We immediately write every element in parallel below before any read.
    unsafe { arr.set_len(n) };

    // Transmit pointer as usize to satisfy the 'static + Send bound on thread::spawn.
    // Safety: each thread accesses a disjoint [start, end) slice; all threads are
    // joined before this function returns, guaranteeing arr outlives them.
    let ptr = arr.as_mut_ptr() as usize;
    let chunk = n / num_threads;
    let pins = build_pin_list(strategy, num_threads, core_ids);

    let handles: Vec<_> = (0..num_threads)
        .map(|tid| {
            let pin = pins[tid];
            thread::spawn(move || {
                // ── Pin this thread to its assigned core ──────────────────
                if let Some(core) = pin {
                    core_affinity::set_for_current(core); // <── 1 line
                }
                // ─────────────────────────────────────────────────────────
                let start = tid * chunk;
                let end = if tid == num_threads - 1 { n } else { start + chunk };
                // Safety: disjoint slices, arr outlives all threads (joined below).
                let slice = unsafe { std::slice::from_raw_parts_mut(ptr as *mut u64, n) };
                for i in start..end {
                    slice[i] = i as u64; // first-touch: page allocated on this thread's NUMA node
                }
            })
        })
        .collect();

    for h in handles {
        h.join().unwrap();
    }
}

/* ── Parallel sum ────────────────────────────────────────────────────────── */

fn parallel_sum(arr: Arc<Vec<u64>>, num_threads: usize, strategy: &str, core_ids: &[CoreId]) -> u64 {
    let n = arr.len();
    let chunk = n / num_threads;
    let pins = build_pin_list(strategy, num_threads, core_ids);

    let handles: Vec<_> = (0..num_threads)
        .map(|tid| {
            let arr = Arc::clone(&arr);
            let pin = pins[tid];
            thread::spawn(move || {
                // ── Pin this thread to its assigned core ──────────────────
                if let Some(core) = pin {
                    core_affinity::set_for_current(core); // <── 1 line
                }
                // ─────────────────────────────────────────────────────────
                let start = tid * chunk;
                let end = if tid == num_threads - 1 { n } else { start + chunk };
                arr[start..end].iter().sum::<u64>()
            })
        })
        .collect();

    handles.into_iter().map(|h| h.join().unwrap()).sum()
}

/* ── Main ────────────────────────────────────────────────────────────────── */

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let mut num_threads = 8usize;
    let mut n = N_DEFAULT;
    let mut strategy = "default".to_string();
    let mut trials = 5usize;
    let mut warmup = false;
    let mut csv = false;

    let mut pos = 0usize;
    for arg in &args[1..] {
        match arg.as_str() {
            "--csv"    => csv    = true,
            "--warmup" => warmup = true,
            _ => {
                pos += 1;
                match pos {
                    1 => num_threads = arg.parse().unwrap_or(num_threads),
                    2 => n           = arg.parse().unwrap_or(n),
                    3 => strategy    = arg.clone(),
                    4 => trials      = arg.parse().unwrap_or(trials),
                    _ => {}
                }
            }
        }
    }

    if !["default", "spread", "close"].contains(&strategy.as_str()) {
        eprintln!("Unknown strategy '{}'. Use: default | spread | close", strategy);
        std::process::exit(1);
    }

    // Enumerate available cores once; used by build_pin_list in every call.
    let core_ids: Vec<CoreId> = core_affinity::get_core_ids().unwrap_or_default();

    if core_ids.is_empty() && strategy != "default" {
        eprintln!("Warning: core_affinity returned no cores; falling back to default.");
    }

    // Allocate without initializing (first-touch done in parallel below).
    let mut arr: Vec<u64> = Vec::with_capacity(n);

    // Parallel first-touch init: pages allocated on threads' local NUMA nodes.
    parallel_init(&mut arr, num_threads, &strategy, &core_ids);

    // Wrap in Arc for shared read access across sum threads.
    let arr = Arc::new(arr);

    // Optional warmup (not timed).
    if warmup {
        parallel_sum(Arc::clone(&arr), num_threads, &strategy, &core_ids);
    }

    if csv {
        println!("trial,threads,n,strategy,elapsed_sec,bandwidth_GBs,checksum");
    }

    let bytes = (n * std::mem::size_of::<u64>()) as f64;

    for t in 1..=trials {
        let t0 = Instant::now();
        let result = parallel_sum(Arc::clone(&arr), num_threads, &strategy, &core_ids);
        let elapsed = t0.elapsed().as_secs_f64();
        let bw_gbs = bytes / elapsed / 1e9;

        if csv {
            println!(
                "{},{},{},{},{:.9},{:.3},{}",
                t, num_threads, n, strategy, elapsed, bw_gbs, result
            );
        } else {
            println!(
                "Trial {}: {:.1} ms  {:.2} GB/s  (sum={})",
                t,
                elapsed * 1000.0,
                bw_gbs,
                result
            );
        }
    }
}
