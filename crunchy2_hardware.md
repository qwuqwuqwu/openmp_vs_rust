# Crunchy2 Hardware Information

All benchmarks in this project are run on the NYU CIMS crunchy2 node.
This file records the hardware specification for reproducibility and context.

---

## CPU Summary

| Property | Value |
|---|---|
| Architecture | x86_64 |
| Vendor | AMD |
| Model | AMD Opteron(TM) Processor 6272 |
| CPU family | 21 |
| Total logical CPUs | 64 |
| Sockets | 4 |
| Cores per socket | 16 |
| Threads per core | 1 (no hyperthreading) |
| Total physical cores | 64 |
| BogoMIPS | 4200.15 |

> **No hyperthreading.** Each logical CPU is a distinct physical core.
> Thread counts in benchmarks map directly to physical cores.

---

## Cache Hierarchy

| Level | Total size | Instances |
|---|---|---|
| L1 data | 1 MiB | 64 (one per core) |
| L1 instruction | 2 MiB | 32 |
| L2 | 64 MiB | 32 (shared per 2 cores) |
| L3 | 48 MiB | 8 (shared per 8 cores) |

---

## NUMA Topology

The machine has **8 NUMA nodes** across 4 sockets. Each socket contains 2 NUMA nodes of 8 cores each.

| NUMA node | CPU cores |
|---|---|
| node0 | 0–7 |
| node1 | 8–15 |
| node6 | 16–23 |
| node7 | 24–31 |
| node2 | 32–39 |
| node3 | 40–47 |
| node4 | 48–55 |
| node5 | 56–63 |

> **Important for benchmarks:** When thread counts exceed 8, threads will span
> multiple NUMA nodes. Cross-NUMA memory access is significantly slower than
> local access. Scalability results at 16+ threads may reflect NUMA effects
> rather than pure parallelism overhead. This is worth noting in the analysis
> when speedup flattens or drops at higher thread counts.

---

## Instruction Set Extensions

Notable flags relevant to this project:

| Feature | Present |
|---|---|
| SSE / SSE2 / SSE4.1 / SSE4.2 | Yes |
| AVX | Yes |
| AES | Yes |
| x86-64 (64-bit mode) | Yes |
| AMD-V (hardware virtualization) | Yes |

---

## Raw `lscpu` Output

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
    BogoMIPS:                4200.15
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
