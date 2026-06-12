#!/bin/bash

echo "Caching sudo credentials..."
sudo -v

OUT_DIR="$HOME/perf-results/gemm-mem-profiles/kvm"
mkdir -p "$OUT_DIR"

echo "======================================================"
echo " Starting Pure perf mem GEMM Benchmark (N=256)"
echo " Output Directory: $OUT_DIR"
echo "======================================================"

MEM_DATA="${OUT_DIR}/kvm_mem_gemm_256.data"
MEM_REPORT="${OUT_DIR}/kvm_mem_report_gemm_256.txt"
WORKLOAD_OUT="${OUT_DIR}/kvm_workload_gemm_256.txt"

N_SIZE=256

echo ">>> Tracking Micro Memory Latency (perf mem record)..."

sudo perf mem record -t load,store -C 2 -o "$MEM_DATA" -- sleep 9999 &
MEM_PID=$!

sleep 2

ssh -i ~/.ssh/id_lab -p 2222 root@localhost "./benchmarks/pc-mm gemm $N_SIZE" >"$WORKLOAD_OUT" 2>&1

sudo kill -INT $MEM_PID

sleep 3

echo ">>> Translating binary data to readable report..."
sudo perf mem report -i "$MEM_DATA" --stdio >"$MEM_REPORT"

sudo rm -f "$MEM_DATA"

echo "======================================================"
echo " GEMM N=256 Complete!"
echo " Check the printed program output in: $WORKLOAD_OUT"
echo " Check your memory latency table in: $MEM_REPORT"
echo "======================================================"
