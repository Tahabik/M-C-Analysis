# M-C-Analysis
Analysis of Memory and Cache performances in Linux When working with a memory intensive program 

---

**Memory & Cache Architecture and Hierarchy in modern CPU**

![memory & cache hierarchy](./image.png)


+ L1 cache:
  + Size: Smallest cache (16KB - 128KB)
  + Associativity: has the associativity between 2-8 ways
  + Latency: Fastest cache, 1 - 3 cycles
+ L2 cache:
  + Size: Slightly larger than L1 cache, 256KB - 2MB
  + Associativity: 4 - 16 ways
  + Latency: 4 - 10 cycles
+ L3 cache:
  + Size: Largest cache in most architectures, 2MB - 32MB per core
  + Associativity: 16 ways
  + Latency: Longest, 10 Cycles - 40 Cycles 

**What are cache misses**


In several scenarios, cache misses might appear inconsequential at first glance, but can lead to substantial performance degradation. When our program encounters frequent cache misses, it waits to fetch data from the main memory. This causes unnecessary execution delays. Therefore, effectively monitoring and mitigating cache misses becomes important


**My Setup:**

I'm using two VM one of them is a arm64 debian 13 and the othe rone is an amd64 debian 13 and they are being hosted on an amd64 debian 13 host:

![VM setup](./image%20copy.png)

---

# Memory and Cache performance analysis using `perf`:

+ `perf mem`
  

+ `perf stat`
  + `perf stat -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations <some command run (sleep 5)>`
    + `-e` -> allows us to specify a set of events we wish to monitor
    + 

---

# Systems memory structure analysis

> This part can differ for different systems (It is my own system's description)


Well first of all let's check the caches' size of our hosts:

by using the command below we can check the caches' size in our host machine:

```bash
lscpu | grep -i cache
```

The result (It's for my hardware only ):

```

L1d cache:                               352 KiB (10 instances)                    
L1i cache:                               576 KiB (10 instances)                    
L2 cache:                                6.5 MiB (4 instances)                     
L3 cache:                                12 MiB (1 instance)   

```
+‌ L1d Cache (352 KiB, 10 instances): This is the level 1 Data cache. It stores the actual data our CPU is actively processing

+ L1i -(575 KiB, 10 instances) -> This is the level 1 instruction cache. It stores the code/instructions telling the CPU waht to do with the data
  + "10 instances" -> means ew have 10 distinct physical CPU cores that each get their very own dedicated L1 cache.
+ L2 cache (6.5 MiB, 4 instances)
  + "4 instances" -> our CPU architecture groups cores together. Instead of every core getting its own L2, clusters of cores share on L2 cache pool, or we have a mix of performance and efficiency cores sharing them

Something funny I've faced with, I have 12 core cpu based on the command `nproc` but in the above report we're observing that we have 10 instances of L1 cache and we've gotta ask this question: WAS IT NOT SUPPOSED TO BE ONE L1 CACHE PER EACH CORE?

> The funny thing is that when `nproc` returns 12, it isn't telling us how many physical cores we have; it's telling us how many **logical cores(processing threads)**  Linux can schedule tasks on.
> Our CPU actually has exactly 10 physical cores, which perfectly matches our 10 instances L1 cahce.

Intel splits these 10 physical cores into two different teams to balance power and battery life:
1. 2 Performance Cores (P-Cores): These are beefy, high-speed cores meant for heavy lifting. They feature Hyper-Threading, meaning each physical core acts as 2 logical threads (2*2 = 4).
2. 8 Efficient Cores (E-Cores): These handle lighter background tasks to save energy. They do not have Hyper-Threading, so 1 physical core equals exactly 1 logical thread(8 * 1 = 8).

Why L2 cache has only 4 instances:
E-cores are grouped into "clusters" of 4, and each cluster shares a single block of L2 cache:
+ 2 instances of L2 cache (1 dedicated to each of the 2 P-cores)
+ 2 instances of L2 cache (1 shared for each cluster of 4 E-cores, covering all 8 E-cores)
+ Total = 4 instances of L2 cache!

> For more detailed report you can use this command: `sudo dmidecode -t cache`.

# Putting Load on VMs scenarios


**Scenario 1: EPT/Shadow Page Table Walk Amplification Under Two-Level TLB Pressure**


First before getting ahead of ourselves let's check what is EPT and NPT!!!!

EPT -> Extended Page Tables: is a hardware virtualization extension that eliminates the need for software emulation of page tables. In short, the technology allows a guest virtual machine to directly manage its own memory while the hypervisor retains full control over the servers physical memory, offloading work from the centroal processor.
This technology is used in KVM. This technology is important for near native performance.

> Support for EPT is built into Intel processors starting with the Nehalem microarchitecture and into AMD processors featuring Rapid Virtualization Indexing.

How it works?

The working principle of it is based on two stage address translation. In traditional virtualization without hardware support, the hypervisor must intercept every attempt by the guest operating system to modify its page tables and substitute the guests physical addresses with real machine addresses by maintaining shadow structures. Hardware support eliminates this shadow layer. When EPT is enabled, the guest system freely works with its own virtual address space, using its own page tables to translate a process virtual address into a so called guest physical address. The processor memory manageemnt unit does not stop at this result but launches a second independent translation stage. A special register points to the root of the EPT tables, which belong exclusively to the hypervisor. 




We first have to construct a memory access pattern that maximizes EPT walks by deliberately defeating the TLB on both levels (guest TLB + host EPT TLB).



Briefing:
> When a guest virtual address is accessed, the CPU must translate it to a physical host address through two independent page table hierarchies. This creates an amplification effect: one guest TLB miss can trigger up to 24 memory accesses for the full EPT walk. This scenario makes the cost visible and measurable.

![Two-level translation problem](./image%20copy%202.png)


On the host we disable THP(Transparent Huge Pages) first to force 4K PTEs:

This forces the host kernel to back guest memory with 4K PTEs instead of 2 MB hugepages. Without this, THP might silently collapse our guest RAM into huge pages, masking the exact EPT amplification we are trying to measure.


```bash
echo never > /sys/kernel/mm/transparent_hugepage/enabled

```

---


## Deterministic Host Configuration & CPU/Cache Pinning Blueprint

When we run a memory tracing, we want every cache miss, every TLB fault, every page walk latency we measure to because by the virtualization mechanism under test, not by the host OS scheduler accidentally moving a vCPU thread from core 2 to core 5 mid-benchmark, evicting our carefully warmed L2 cache in the process.


There are three distinct interference sources we're eliminating, in order of severity:

1. **Cache Eviction from Thread Migration** If the host scheduler migrates a QEMU vCPU thread between physical cores, the L1 and L2 caches on the original core go cold. O next cache miss measurement now includes "cache cold from migration" noise on top of the actual virtualization overhead we're trying to measure. L1/L2 are per-core, crossing cores means starting from scratch.

2. **Shared Core Interference** If a host OS thread (say, a kernel worker, `kworker`, or our SSH session) shares a physical core with a vCPU thread, it competes for the same L1/L2 cache. Every time that host thread runs, it evicts guest working-set cache lines. Our LLC miss numbers then reflect host thread pullution, not guest behavior.

3. **Cross-NUMA Node Memory Latency** If our vCPU thread is pinned to a core on NUMA node 0, but the physical memory backing the guest RAM was allocated on NUMA node 1, every single guest memory access pays a remote NUMA penalty (typically 30-100ns extra per access). This completely drowns out the EPT vs SoftMMU signal we're trying to isolate.

**The end state we're building toward**:

```
Physical Core 0-1 -> Host OS, IRQs, SSH, everything else
Physical Core 2-3 -> KVM VM vCPU threads ONLY
Physical Core 4-7 -> QEMU TCG vCPU threads ONLy(TCG needs more cores - software emulation is heavier)

All memory for both VMs -> allocated on the SAME NUMA node as their pinned cores.

```


### Understanding Physical Hardware First:

To pin CPU intelligently we have to know the actual topology of our machine we need to answer four questions:

+ How many physical cores do we have and how are they numed.
+ which cores share L2 caches with each other
+ How many NUMA nodes exist, and which cores belong to each 
+ Are there hyperthreads, and if so what is the sibling mapping.

```bash
lstopo
```
![cpu architecture](./image%20copy%203.png)




```bash
nproc --all
```
```
12
```


```bash
lscpu | grep -E "^CPU\(s\)|^Core|^Socket|^Thread|^NUMA|Model name"
```
```
CPU(s):                                  12
Model name:                              12th Gen Intel(R) Core(TM) i5-1235U
Thread(s) per core:                      2
Core(s) per socket:                      10
Socket(s):                               1
CPU(s) scaling MHz:                      32%
NUMA node(s):                            1
NUMA node0 CPU(s):                       0-11
```

```bash
cat /sys/devices/system/cpu/cpu0/cache/index2/shared_cpu_list
```
```
0-1
```

```bash
numactl --hardware
```
```
available: 1 nodes (0)
node 0 cpus: 0 1 2 3 4 5 6 7 8 9 10 11
node 0 size: 15698 MB
node 0 free: 5580 MB
node distances:
node     0 
   0:   10 
```

We have an Intel 12th Gen i5-1235U. Looking at the `lstopo`diagram reveals it clearly.

Cores 0 and 1 are performance cores. Each has 2 hyperthreads. L1d is 48 KB, L1i is 32KB, L2 is 1280KB.

Cores 2-9:
These are efficiency cores, each has only 1 logical CPU, no hyperthreading. L1d is 32KB and L1i is 64KB. Notice the L2 grouping from the diagram: cores 2-5 share one 2048KB L2, and cores 6-9 share another 2048KB L2.

To prevent the KVM VM's cache activity from pollyting the TCG VM's L2 and vice versa, we should put each VM in a different L2 clusters.


Based on the explanations the scheme we're going to have is like below:
```
┌──────────────────────────────────────────────────────────────────────┐
│  CPU0+CPU1  (P-core #0 HT pair, L2: 1280KB)  ─┐                      │
│  CPU2+CPU3  (P-core #1 HT pair, L2: 1280KB)  ─┴──  HOST ZONE         │
│                                                    IRQs, kernel,     │
│                                                    SSH, desktop      │
├──────────────────────────────────────────────────────────────────────┤
│  CPU4+CPU5+CPU6+CPU7  (E-cluster A, L2: 2048KB) ──  KVM ZONE         │
│                                                    2 vCPUs pinned    │
│                                                    here, L2 is       │
│                                                    private to KVM    │
├──────────────────────────────────────────────────────────────────────┤
│  CPU8+CPU9+CPU10+CPU11  (E-cluster B, L2: 2048KB) ── TCG ZONE        │
│                                                    2 vCPUs + helper  │
│                                                    threads pinned    │
│                                                    here, L2 private  │
│                                                    to TCG            │
└──────────────────────────────────────────────────────────────────────┘
```


In this scheme, each VM gets its own dedicated L2 cluster with zero sharing with the other VM or the host. The two VM cannot evict each other's cache lines at L2. The host P-cores have their own seperate L2s entirely. The L3 (12MB) is still shared across averything. That is unavoidable on a single-socket cunsumer CPU, but L1 and L2 are now fully isolated per zone.


**Now it's time for Isolation baby!!**

Now here we are going to write the actual isolation parameters.

we should change the `/etc/default/grub` file:

We should add this line:
```
GRUB_CMDLINE_LINUX="isolcpus=4-11 nohz_full=4-11 rcu_nocbs=4-11 irqaffinity=0-3"
```

+ `isolcpus=4-11` -> Remove CPUs 4-11 from the kernel's general purpose scheduler domain. the scheduler will not place any tasks on these CPUs.
+ `nohz_full=4-11` -> Normally the kernel fires a scheduler tick interrupt (typically 250 Hz on Debian) on every CPU to preempt tasks and rebalance load. On isolated CPUs running a single pinned task, this tick causes unnecessary interruptions. `nohz_full` suppresses this tick when only task in running on that CPU, reducing interrupt-driven cache pollution.
+ `rcu_nocbs=4-11` -> Read-Copy-Update (RCU) is a kernel synchronization mechanism that prediocally runs callback work on CPUs. Without this parameters, RCU callbacks can fire on our isolated CPUs mid-benchmark, evicting cache lines. This offloads those callbacks to non-isolated CPUs
+ `irqaffinity=0-3` -> Hardware interrupt delivery defaults to any CPU. This pins all IRQ delivery to only CPUs 0-3 (our host zone), ensuring no device interrupt wakes up and runs on our VM-dedicated CPUs.

and for it to apply we shoudl update the grub configurations and reboot our system


To check if the isolation has happened we can use the below commands:
```bash

cat /proc/cmdline

cat /sys/devices/system/cpu/isolated
cat /sys/devices/system/cpu/nohz_full

ps -eo psr,pid,comm --no-headers | sort -n | awk '
{count[$1]++}
END {for (cpu in count) printf "CPU%s: %d threads currently scheduled\n", cpu, count[cpu]}
' | sort -t'U' -k1 -n

cat /proc/interrupts | awk 'NR==1{print; next} {
    split($0, a, " "); 
    total=0; for(i=2;i<=13;i++) total+=a[i]; 
    if(total>0) print
}' | head -30
```

As we check the interrupts, we check that nearly all the interrupts happen in the cores 0-3 buttt there are some interrupts that happens in cores that were not suppose to happen.
Wifi driver queues (IRQs 176-183) are currently delivering interrupts onto CPUs4-11 which is in our isolation zones. This happens because the WiFi driver (`iwlwifi`) creates one interrupt queue per CPU at load time, ignoring `irqaffinity`. 

Moving all iwlwifi IRQs back to CPUs 0-3:
```bash
# Find all iwlwifi IRQ numbers
for irq in $(grep iwlwifi /proc/interrupts | awk -F: '{print $1}' | tr -d ' '); do
    echo -n "Moving IRQ $irq to CPUs 0-3: "
    echo "f" | sudo tee /proc/irq/${irq}/smp_affinity
done
```


> just one other thing the `nocloud` images does not have default passowrd and. If we boot them as is we'll get a login prompt with no credentials. The simplest thing to do is to set a root password directly into the image before booting:

```bash
sudo virt-customize -a /home/taha/Public/Distros/kvm-lab.qcow2 \
    --root-password password:trace \
    --hostname kvm-lab \
    --run-command 'systemctl enable serial-getty@ttyS0.service'

sudo virt-customize -a /home/taha/Public/Distros/tcg-lab.qcow2 \
    --root-password password:trace \
    --hostname tcg-lab \
    --run-command 'systemctl enable serial-getty@ttyS0.service'

```

**Launching the VMs**:

KVM VM:

```bash
sudo qemu-system-x86_64 \
  -enable-kvm \
  -cpu host \
  -smp 2,sockets=1,cores=2,threads=1 \
  -m 2G \
  -drive file=/home/taha/Public/Distros/kvm-lab.qcow2,format=qcow2,if=virtio \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS_4M.fd \
  -nographic \
  -serial mon:stdio \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -name "kvm-lab,process=kvm-lab" \
  -pidfile /tmp/kvm-lab.pid
```

+ enable-kvm          → activates hardware virtualization via /dev/kvm
+ cpu host            → guest CPU = exact copy of host CPU (exposes real PMU events)
+ smp 2               → 2 vCPUs (maps to 2 host threads we will pin to CPUs 4,5)
+ m 2G                → 2GB RAM (enough for workloads, leaves room for TCG)
+ nographic           → no display window, all output via serial
+ serial mon:stdio    → serial console + QEMU monitor in the same terminal



TCG VM:
```bash
sudo qemu-system-aarch64 \
  -machine virt,gic-version=3 \
  -cpu cortex-a72 \
  -smp 2,sockets=1,cores=2,threads=1 \
  -m 2G \
  -drive file=/home/taha/Public/Distros/tcg-lab.qcow2,format=qcow2,if=virtio \
  -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
  -nographic \
  -serial mon:stdio \
  -netdev user,id=net0,hostfwd=tcp::2223-:22 \
  -device virtio-net-pci,netdev=net0 \
  -name "tcg-lab,process=tcg-lab" \
  -pidfile /tmp/tcg-lab.pid
```

+ machine virt        → ARM64 platform (no KVM flag = TCG automatically)
+ cpu cortex-a72      → emulate a specific ARM core (deterministic instruction set)
+ smp 2               → 2 guest vCPUs (TCG uses more host threads than this)
+ m 2G                → 2GB RAM
+ nographic + serial  → same console approach

Every VM is represented with a process now we're going to check what their process id is on the host machine:

```bash
cat /tmp/kvm-lab.pid
cat /tmp/tcg-lab.pid
```

And the best practice can be that we set the process ids as environment variables.


Now these VM that we have created is supported by bunch of proesses and we're gonna see which processes there are, for observing these processes we're gonna use the command below:

```bash
ps -T -p <KVM/TCG_PID> -o spid,comm 
```

> SPID -> System process ID, (or more accurately, Thread ID) -> Unique identifier assigned by the linux kernel to an individual kernel rather than overall process.

### Scenario. Cache Hierarchy Stress (Cache line bouncing & LLC Eviction Patterns)

In this scenario we tend to create a working set that exceeds L3 cache size using a memory access pattern that produces maximum cache line utilization contrast between native and emulated execution.

**Why it differentiates KVM vs TCG:**
+ In the KVM case, guest cache behavior maps almost 1:1 to host physical cache behavior. The hardware prefetcher operates normally; we'll see the real cache hierarchy underload.
+ In the Qemu TCG case, cache behavior is fundamentally distorted. Every memory access instruction in the ARM64 guest is translated by TCG inot an x86 instruction sequence that includes a software TLB lookup before the actual memory access. This means the actual memory access pattern seen by the host's L1/L2/L3 hardware caches includes both the guest's intended access AND the TCG metadata accesses (TLB table enteries, TranslationBlock structures), polluting the cache footprint substantially.