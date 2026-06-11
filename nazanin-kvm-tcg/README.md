
## Run
./run-x86.sh   # start x86 VM
./run-arm.sh   # start ARM VM

## Benchmark

Uses the same benchmark from `mili/main/benchmark.c` (pointer chasing only).

## Analyze
./analyze_x86.sh   # collect perf data from x86 VM
./analyze_arm.sh   # collect perf data from ARM VM

## Output files
- full_stat.log: raw perf counters
- hotspots.txt: functions with most cache misses
- flamegraph.svg: visual hotspot analysis