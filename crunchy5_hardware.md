# Crunchy5 Hardware Information

All benchmarks in this project are run on the NYU CIMS crunchy5 node.
This file records the hardware specification for reproducibility and context.

---

## CPU Summary

| Property | Value |
|---|---|
| Architecture | x86_64 |
| Vendor | AMD |
| Model | AMD Opteron(TM) Processor 6272 |
| CPU family | 21 (Bulldozer / Interlagos) |
| Model number | 1 |
| Stepping | 2 |
| Total logical CPUs | 64 |
| Sockets | 4 |
| Cores per socket | 16 |
| Threads per core | 1 (no hyperthreading) |
| Total physical cores | 64 |
| BogoMIPS | 4200.22 |

> **No hyperthreading.** Each logical CPU is a distinct physical core.
> Thread counts in benchmarks map directly to physical cores.

---

## Cache Hierarchy

| Level | Total size | Instances | Scope |
|---|---|---|---|
| L1 data | 1 MiB | 64 | One per core |
| L1 instruction | 2 MiB | 32 | Shared per 2-core module |
| L2 | 64 MiB | 32 | Shared per 2-core module |
| L3 | 48 MiB | 8 | Shared per 8-core die (6 MiB each) |

> **Bulldozer note:** The Opteron 6272 uses the Bulldozer microarchitecture where
> each "module" contains 2 integer cores sharing one FPU and one L1i/L2 cache.
> This is why L1i and L2 have 32 instances for 64 cores.

---

## NUMA Topology

The machine has **8 NUMA nodes** across 4 sockets. Each socket (2 dies) contributes
2 NUMA nodes of 8 cores each. The NUMA node numbering is **non-sequential** with
respect to physical cores — note that nodes 6 and 7 interleave with nodes 0 and 1
within the first two sockets.

### Core-to-NUMA-node mapping

| NUMA node | CPU cores | Socket | Die |
|-----------|-----------|--------|-----|
| node 0 | 0–7 | 0 | 0 |
| node 1 | 8–15 | 0 | 1 |
| node 6 | 16–23 | 1 | 0 |
| node 7 | 24–31 | 1 | 1 |
| node 2 | 32–39 | 2 | 0 |
| node 3 | 40–47 | 2 | 1 |
| node 4 | 48–55 | 3 | 0 |
| node 5 | 56–63 | 3 | 1 |

> **Important for affinity benchmarks:** When using `OMP_PLACES=cores` with
> `proc_bind(close)`, threads fill cores in numeric order (0, 1, 2, ...).
> Due to the non-sequential NUMA numbering, 32 threads (cores 0–31) land on
> nodes 0, 1, 6, 7 — **not** nodes 0–3 as one might assume.

---

## Memory Configuration

From `numactl -H` (snapshot taken April 10, 2026):

### Per-node capacity and free memory

| NUMA node | Size (MB) | Size (approx) | Free (MB) at snapshot |
|-----------|-----------|---------------|----------------------|
| node 0 | 31,614 | ~32 GB | 25,060 MB |
| node 1 | 32,253 | ~32 GB | 29,240 MB |
| node 2 | 32,253 | ~32 GB | 10,995 MB |
| node 3 | 32,253 | ~32 GB | 23,322 MB |
| node 4 | 32,253 | ~32 GB | 11,979 MB |
| node 5 | 32,253 | ~32 GB | 20,820 MB |
| node 6 | 32,253 | ~32 GB | 10,688 MB |
| node 7 | 32,234 | ~32 GB | 13,852 MB |
| **Total** | **256,365 MB** | **~256 GB** | |

> Node 0 has 31,614 MB rather than 32,253 MB because a small region is reserved
> for the kernel and hardware memory-mapped I/O.

### Hardware memory specification (AMD Opteron 6272)

| Property | Value |
|---|---|
| Memory type | DDR3 |
| Memory channels per die | 2 |
| Theoretical peak per channel | 12.8 GB/s (DDR3-1600) |
| Theoretical peak per NUMA node | ~25.6 GB/s (2 × 12.8) |
| Measured peak per node (B5) | ~15–16 GB/s (~62% efficiency) |
| Total theoretical peak (8 nodes) | ~204.8 GB/s |
| Measured total peak (B5, OMP close 64T) | ~122–128 GB/s (~60–63% efficiency) |

The 60–65% efficiency is typical for real streaming workloads on DDR3 systems
(the STREAM benchmark routinely achieves this fraction of theoretical DDR peak).

---

## NUMA Distance Matrix

From `numactl -H`. Distance is a relative latency metric: local access = 10 (baseline),
all other values are multiples of local latency.

```
        node0  node1  node2  node3  node4  node5  node6  node7
node0:    10     16     16     22     16     22     16     22
node1:    16     10     22     16     16     22     22     16
node2:    16     22     10     16     16     16     16     16
node3:    22     16     16     10     16     16     22     22
node4:    16     16     16     16     10     16     16     22
node5:    22     22     16     16     16     10     22     16
node6:    16     22     16     22     16     22     10     16
node7:    22     16     16     22     22     16     16     10
```

### Distance tiers

| Distance | Meaning | Latency multiplier |
|---|---|---|
| 10 | Local (same NUMA node) | 1.0× baseline |
| 16 | Near-remote (1 HyperTransport hop) | ~1.6× |
| 22 | Far-remote (2 HyperTransport hops) | ~2.2× |

### Analysis of the distance structure

The Opteron 6272 uses AMD's HyperTransport (HT) interconnect. Each socket has HT links
to the other three sockets. The distances reflect the number of hops required:

- **Distance 16 (1 hop):** Nodes on directly connected sockets — one HT link traversal.
- **Distance 22 (2 hops):** Nodes requiring a relay through an intermediate socket —
  two HT link traversals.

The matrix is **not symmetric in terms of which node-pairs are near vs far**. For example:
- node 0 ↔ node 5: distance **22** (far)
- node 0 ↔ node 2: distance **16** (near)
- node 2 ↔ node 7: distance **16** (near)
- node 3 ↔ node 7: distance **22** (far)

This means a thread on core 0 (node 0) reading memory on node 5 pays a 2.2× latency
penalty, while reading from node 2 pays only 1.6×. This asymmetry creates unpredictable
bandwidth depending on which nodes a workload happens to touch.

### Near-remote pairs (distance 16)

node0–node1, node0–node2, node0–node4, node0–node6,
node1–node3, node1–node4, node1–node7,
node2–node3, node2–node4, node2–node5, node2–node6, node2–node7,
node3–node4, node3–node5,
node4–node5, node4–node6,
node6–node7

### Far-remote pairs (distance 22)

node0–node3, node0–node5, node0–node7,
node1–node2, node1–node5, node1–node6,
node3–node6, node3–node7,
node4–node7,
node5–node6, node5–node7

---

## Instruction Set Extensions

| Feature | Present | Relevance |
|---|---|---|
| SSE / SSE2 | Yes | 128-bit SIMD; used by both GCC and LLVM in B5 |
| SSE4.1 / SSE4.2 | Yes | Integer dot products, string ops |
| AVX | Yes (flag present) | 256-bit SIMD — **not used** by GCC/LLVM on this CPU |
| AES | Yes | Hardware AES acceleration |
| FMA4 | Yes | AMD-specific fused multiply-add (4-operand form) |
| XOP | Yes | AMD extended operations (integer SIMD) |
| POPCNT | Yes | Hardware population count (used in B3) |
| AMD-V | Yes | Hardware virtualization |

> **AVX note:** Although the AVX flag is present, the Opteron 6272's AVX
> implementation executes 256-bit operations as two 128-bit micro-ops internally
> (no native 256-bit datapath). GCC and LLVM both default to SSE2/128-bit for
> this CPU because 256-bit AVX provides no throughput benefit.

---

## Security Mitigations Active

The following CPU vulnerability mitigations are active and may marginally affect
benchmark timing:

| Mitigation | Status |
|---|---|
| Retbleed | Mitigated — untrained return thunk |
| Spectre v1 | Mitigated — usercopy/swapgs barriers |
| Spectre v2 | Mitigated — Retpolines + IBPB conditional |
| Spec Store Bypass | Mitigated — disabled via prctl |
| Meltdown, MDS, L1tf | Not affected (AMD) |

Retpoline (Spectre v2 mitigation) replaces indirect branches with a return-based
trampoline. This can affect code with many indirect calls — relevant for OMP's
`GOMP_barrier@plt` PLT dispatch (B1) but negligible for the tight loops in B5.

---

## Raw Command Output

### `lscpu`

```
Architecture:                x86_64
  CPU op-mode(s):            32-bit, 64-bit
  Address sizes:             48 bits physical, 48 bits virtual
  Byte Order:                Little Endian
CPU(s):                      64
  On-line CPU(s) list:       0-63
Vendor ID:                   AuthenticAMD
  Model name:                AMD Opteron(TM) Processor 6272
    CPU family:              21
    Model:                   1
    Thread(s) per core:      1
    Core(s) per socket:      16
    Socket(s):               4
    Stepping:                2
    BogoMIPS:                4200.22
    Flags:                   fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge
                             mca cmov pat pse36 clflush mmx fxsr sse sse2 ht
                             syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm
                             constant_tsc rep_good nopl nonstop_tsc cpuid
                             extd_apicid amd_dcm aperfmperf pni pclmulqdq monitor
                             ssse3 cx16 sse4_1 sse4_2 popcnt aes xsave avx
                             lahf_lm cmp_legacy svm extapic cr8_legacy abm sse4a
                             misalignsse 3dnowprefetch osvw ibs xop skinit wdt
                             fma4 nodeid_msr topoext perfctr_core perfctr_nb cpb
                             hw_pstate pti ssbd ibpb vmmcall arat npt lbrv
                             svm_lock nrip_save tsc_scale vmcb_clean flushbyasid
                             decodeassists pausefilter pfthreshold
Virtualization:              AMD-V
Caches (sum of all):
  L1d:                       1 MiB (64 instances)
  L1i:                       2 MiB (32 instances)
  L2:                        64 MiB (32 instances)
  L3:                        48 MiB (8 instances)
NUMA:
  NUMA node(s):              8
  NUMA node0 CPU(s):         0-7
  NUMA node1 CPU(s):         8-15
  NUMA node2 CPU(s):         32-39
  NUMA node3 CPU(s):         40-47
  NUMA node4 CPU(s):         48-55
  NUMA node5 CPU(s):         56-63
  NUMA node6 CPU(s):         16-23
  NUMA node7 CPU(s):         24-31
```

### `numactl -H`

```
available: 8 nodes (0-7)
node 0 cpus: 0 1 2 3 4 5 6 7
node 0 size: 31614 MB
node 0 free: 25060 MB
node 1 cpus: 8 9 10 11 12 13 14 15
node 1 size: 32253 MB
node 1 free: 29240 MB
node 2 cpus: 32 33 34 35 36 37 38 39
node 2 size: 32253 MB
node 2 free: 10995 MB
node 3 cpus: 40 41 42 43 44 45 46 47
node 3 size: 32253 MB
node 3 free: 23322 MB
node 4 cpus: 48 49 50 51 52 53 54 55
node 4 size: 32253 MB
node 4 free: 11979 MB
node 5 cpus: 56 57 58 59 60 61 62 63
node 5 size: 32253 MB
node 5 free: 20820 MB
node 6 cpus: 16 17 18 19 20 21 22 23
node 6 size: 32253 MB
node 6 free: 10688 MB
node 7 cpus: 24 25 26 27 28 29 30 31
node 7 size: 32234 MB
node 7 free: 13852 MB
node distances:
node     0    1    2    3    4    5    6    7
   0:   10   16   16   22   16   22   16   22
   1:   16   10   22   16   16   22   22   16
   2:   16   22   10   16   16   16   16   16
   3:   22   16   16   10   16   16   22   22
   4:   16   16   16   16   10   16   16   22
   5:   22   22   16   16   16   10   22   16
   6:   16   22   16   22   16   22   10   16
   7:   22   16   16   22   22   16   16   10
```
