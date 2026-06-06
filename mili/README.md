This directory contains Mili's take on the OS project

First you have two bash scripts that run the VMs. You gotta have the Debian 13 nocloud images tho.
I have isolated CPU 0 and CPU 1 here and thats why these scripts do taskset on CPU 0 and 1.

I haven't included the images because they are big in size (duh). You probably have to install gcc, sysbench,
ssh for this setup.

After running the VMs, compile the c benchmarks in the VM (probably better off compiling with `-O2`). You can afterwards run the `collect.sh` that runs the benchmarks and saves them in `results/`. The benchmarks right now include:

- Sysbench (for memory bandwidth and latency)
- Pointer chasing (random & sequential)
- Matrix multiplication (GMM)
- SMC (self modifying code)
