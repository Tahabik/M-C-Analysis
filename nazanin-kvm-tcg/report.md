# Performance Analysis Report: KVM vs TCG Cache Behavior

## 1. Executive Summary

This report compares host CPU cache behavior when running two virtual machines on an Intel Core i7-9850H system:

| VM | Architecture | Accelerator | CPU Pinning |
|----|-------------|-------------|--------------|
| x86 | x86_64 | KVM | Core 0 |
| ARM | aarch64 | TCG | Core 1 |

### Host System Configuration

| Component | Specification |
|-----------|---------------|
| CPU | Intel Core i7-9850H @ 2.60GHz |
| RAM | 30 GB |
| OS | Ubuntu |

### VM Configuration

- **x86 VM:** `--enable-kvm -cpu host -smp 1 -m 2G` (pinned to Core 0)
- **ARM VM:** `-cpu cortex-a72 -smp 1 -m 2G` (pinned to Core 1)

### Benchmark

Only **pointer chasing** benchmark was used:
- Memory sizes: 16KB to 64MB
- Access patterns: Sequential and Random
- Node size: 64 bytes (one cache line)

---

## 2. Key Results (12-second measurement)

| Event | x86 (KVM) | ARM (TCG) | Difference |
|-------|-----------|-----------|-------------|
| instructions | 1.09B | 27.38B | TCG 25x more |
| IPC | 0.092 | 1.299 | TCG 14x higher |
| cycles | 11.87B | 21.07B | TCG 77% more |
| cache miss rate | 22.7% | 19.7% | TCG slightly better |
| dTLB walks | 2.08M | 9.14M | TCG 4.4x more |
| branch-misses | 0.88M | 7.46M | TCG 8.5x more |

---

## 3. Analysis

TCG translates each ARM instruction into multiple simple x86 instructions (MOV, ADD, CMP). This translation results in 25 times more instructions executed on the host compared to KVM, where x86 instructions run directly on the hardware.

The translated instructions are simple and execute in 1-2 cycles each, giving TCG a significantly higher IPC (1.299) compared to KVM (0.092). However, this higher IPC does not indicate faster execution. TCG consumes 77% more total CPU cycles because it executes far more instructions.

The TCG VM shows 4.4 times more TLB walks (9.14M vs 2.08M) and 8.5 times more branch mispredictions (7.46M vs 0.88M). These overheads come from the translation process, including looking up translated blocks, accessing TCG helper functions, and managing the JIT buffer.

The cache miss rate is slightly better for TCG (19.7%) than KVM (22.7%). This is because the JIT buffer stores translated code contiguously, creating a regular memory access pattern with good spatial locality.

Hotspot analysis shows that KVM spends 79% of its time in `main_loop_wait` (I/O waiting), while TCG spends time in translation helpers like `helper_lookup_tb_ptr` and `helper_ldul_mmu`.

TCG also experiences 1,596 page faults during the measurement, while KVM has none. These page faults come from dynamic memory allocation for the JIT buffer as translated code grows.

---

## 4. Conclusion

KVM delivers fewer instructions, fewer TLB walks, fewer branch mispredictions, and 77% fewer CPU cycles. TCG has higher IPC and a slightly better cache miss rate due to simple translated instructions and regular JIT buffer access patterns.

The fundamental difference in host cache behavior comes from TCG's software translation: each ARM instruction becomes multiple x86 instructions. This increases instruction count by 25x, drives up TLB walks and branch mispredictions, and shifts hotspots from I/O waiting (KVM) to translation helpers (TCG).