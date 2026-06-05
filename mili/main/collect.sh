#!/bin/bash

# Configuration PIDs
ARM_PID=$(pgrep -f qemu-system-aarch64)
X86_PID=$(pgrep -f qemu-system-x86_64)

OUTPUT_DIR="results"
mkdir -p "$OUTPUT_DIR"

echo "=== Virtualization/Emulation Benchmarking Data Collector ==="
echo "ARM Guest (Emulation) PID: $ARM_PID"
echo "x86 Guest (KVM) PID: $X86_PID"
echo "Results will be saved in $OUTPUT_DIR"
echo "--------------------------------------------------------"

run_sysbench_sweep() {
  local target=$1 # "arm" or "x86"
  local pid=$2
  local port=$3

  echo "Starting Sysbench Memory Sweep on $target (PID: $pid, Port: $port)..."
  local log_file="$OUTPUT_DIR/sysbench_${target}.log"
  echo "=== Sysbench Memory Sweep: $target ===" >"$log_file"

  for bs in "1K" "4K" "16K" "64K" "256K" "1M" "4M" "16M"; do
    echo "Running block size $bs..." >>"$log_file"
    echo "--- Block Size: $bs ---"

    # We use 1GB total transfer to keep emulation runs reasonable (approx. 5-15s per run)
    local total_size="1G"
    # if [ "$target" = "arm" ]; then
    #   # Emulation is very slow, we can use 512M to make sure it finishes quickly
    #   total_size="512M"
    # fi

    perf stat -e cycles,instructions,cache-misses,L1-dcache-load-misses,L1-icache-load-misses,iTLB-load-misses,dTLB-load-misses -p "$pid" \
      ssh -o StrictHostKeyChecking=no -p "$port" root@127.0.0.1 \
      "sysbench memory --memory-block-size=$bs --memory-total-size=$total_size run" \
      >>"$log_file" 2>&1
  done
  echo "Sysbench sweep on $target completed."
}

run_custom_benchmarks() {
  local target=$1 # "arm" or "x86"
  local pid=$2
  local port=$3

  echo "Running custom Pointer Chasing on $target (PID: $pid, Port: $port)..."
  local chase_log="$OUTPUT_DIR/custom_chase_${target}.log"

  perf stat -e cycles,instructions,cache-misses,L1-dcache-load-misses,L1-icache-load-misses,iTLB-load-misses,dTLB-load-misses -p "$pid" \
    ssh -o StrictHostKeyChecking=no -p "$port" root@127.0.0.1 \
    "/root/benchmark" \
    >"$chase_log" 2>&1

  echo "Running custom Matrix Multiplication (GEMM 256) on $target (PID: $pid, Port: $port)..."
  local gemm_log="$OUTPUT_DIR/custom_gemm_${target}.log"

  perf stat -e cycles,instructions,cache-misses,L1-dcache-load-misses,L1-icache-load-misses,iTLB-load-misses,dTLB-load-misses -p "$pid" \
    ssh -o StrictHostKeyChecking=no -p "$port" root@127.0.0.1 \
    "/root/benchmark gemm 256" \
    >"$gemm_log" 2>&1

  echo "Running custom SMC on $target (PID: $pid, Port: $port)..."
  local smc_log="$OUTPUT_DIR/custom_smc_${target}.log"

  perf stat -e cycles,instructions,cache-misses,L1-dcache-load-misses,L1-icache-load-misses,iTLB-load-misses,dTLB-load-misses -p "$pid" \
    -- ssh -o StrictHostKeyChecking=no -p "$port" root@127.0.0.1 "/root/smc_benchmark 100000" >"$smc_log" 2>&1

  echo "Custom benchmarks on $target completed."
}

# Run everything sequentially
# --- ARM (EMULATION) ---
run_sysbench_sweep "arm" "$ARM_PID" 2223
run_custom_benchmarks "arm" "$ARM_PID" 2223

# --- x86_64 (KVM) ---
run_sysbench_sweep "x86" "$X86_PID" 2222
run_custom_benchmarks "x86" "$X86_PID" 2222

echo "========================================================"
echo "All benchmarks completed successfully! Data collected in $OUTPUT_DIR"
