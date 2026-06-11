#!/bin/bash

RESULTS_DIR="/home/nazanin/qemu-project/results/x86_$(date +%Y%m%d_%H%M%S)"
mkdir -p $RESULTS_DIR

echo "=============================================="
echo "Analysis for x86 VM (KVM) - Pinned to Core 0"
echo "Results saved to: $RESULTS_DIR"
echo "=============================================="

PID=$(pgrep -f "qemu-system-x86_64")
if [ -z "$PID" ]; then
    echo "ERROR: x86 VM is not running!"
    echo "Please start ./run-x86.sh first"
    exit 1
fi
echo "Found x86 VM with PID: $PID"

echo ""
echo "[1/5] perf stat - Full statistics (12 seconds)..."
sudo perf stat -e cycles,instructions,cache-misses,cache-references,LLC-load-misses,LLC-store-misses,L1-dcache-load-misses,l2_rqsts.miss,l2_rqsts.references,dtlb_load_misses.miss_causes_a_walk,branch-misses,page-faults,task-clock -p $PID -- sleep 12 2>&1 | tee $RESULTS_DIR/x86_full_stat.log

echo ""
echo "[2/5] perf record - Sampling cache-misses (12 seconds)..."
sudo perf record -e cache-misses -ag -F 999 -p $PID -- sleep 12 2>/dev/null
sudo perf script > $RESULTS_DIR/x86_out.perf
sudo perf report --stdio -g graph --sort=sym -n > $RESULTS_DIR/x86_hotspots.txt
echo "Top 50 hotspots:"
head -50 $RESULTS_DIR/x86_hotspots.txt

echo ""
echo "[3/5] Generating Flame Graph..."
cd ~/FlameGraph
~/FlameGraph/stackcollapse-perf.pl $RESULTS_DIR/x86_out.perf > $RESULTS_DIR/x86_out.folded 2>/dev/null
~/FlameGraph/flamegraph.pl $RESULTS_DIR/x86_out.folded > $RESULTS_DIR/x86_flamegraph.svg
echo "Flame Graph: $RESULTS_DIR/x86_flamegraph.svg"

echo ""
echo "[4/5] perf stat - Interval statistics (every 500ms)..."
sudo perf stat -e cache-misses,cycles,instructions -I 500 -p $PID -- sleep 12 2>&1 | tee $RESULTS_DIR/x86_interval.log

echo ""
echo "[5/5] perf stat - TLB analysis..."
sudo perf stat -e dTLB-load-misses,dtlb_load_misses.miss_causes_a_walk,page-faults -p $PID -- sleep 12 2>&1 | tee $RESULTS_DIR/x86_tlb.log

echo ""
echo "=============================================="
echo "Results saved in: $RESULTS_DIR"
ls -la $RESULTS_DIR/
echo "=============================================="
