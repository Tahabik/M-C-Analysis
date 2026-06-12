#!/bin/bash

echo "Caching sudo credentials..."
sudo -v

OUT_DIR="$HOME/perf-results/mem-profiles/kvm"
mkdir -p "$OUT_DIR"

echo "======================================================"
echo " Starting Master VM Benchmark Suite (PERF MEM EDITION)"
echo " Output Directory: $OUT_DIR"
echo "======================================================"

echo ""
echo ">>> PHASE 1: Running Default Workload (All sizes, Seq & Rand aggregated)..."

DATA_FILE_DEFAULT="$OUT_DIR/kvm_mem_default.data"
REPORT_FILE_DEFAULT="$OUT_DIR/kvm_mem_report_default.txt"
WORKLOAD_OUT_DEFAULT="$OUT_DIR/kvm_workload_default.txt"

sudo perf mem record -t load,store -C 2 -o "$DATA_FILE_DEFAULT" -- sleep 9999 &
PERF_PID=$!

sleep 2

ssh -i ~/.ssh/id_lab -p 2222 root@localhost "./benchmarks/pc-mm" >"$WORKLOAD_OUT_DEFAULT" 2>&1

sudo kill -INT $PERF_PID
sleep 3

echo "    Parsing binary data into text report..."
sudo perf mem report -i "$DATA_FILE_DEFAULT" --stdio >"$REPORT_FILE_DEFAULT"

sudo rm "$DATA_FILE_DEFAULT"
echo "    Phase 1 Complete."

echo ""
echo ">>> PHASE 2: Starting Isolated Parameter Sweep..."

sizes=(
  $((16 * 1024))
  $((64 * 1024))
  $((256 * 1024))
  $((1024 * 1024))
  $((4 * 1024 * 1024))
  $((16 * 1024 * 1024))
  $((64 * 1024 * 1024))
)

modes=(0 1)

echo "Starting Automated perf mem Sweep..."
echo "-------------------------------------------"

for size in "${sizes[@]}"; do
  for mode in "${modes[@]}"; do
    if [ "$mode" -eq 0 ]; then
      mode_label="seq"
    else
      mode_label="rand"
    fi

    kb_size=$((size / 1024))

    DATA_FILE="$OUT_DIR/kvm_mem_${kb_size}KB_${mode_label}.data"
    REPORT_FILE="$OUT_DIR/kvm_mem_report_${kb_size}KB_${mode_label}.txt"
    WORKLOAD_OUT="$OUT_DIR/kvm_workload_${kb_size}KB_${mode_label}.txt"

    echo ">>> Profiling Size: ${kb_size}KB | Mode: ${mode_label}"

    sudo perf mem record -t load,store -C 2 -o "$DATA_FILE" -- sleep 9999 &
    PERF_PID=$!

    sleep 2

    ssh -i ~/.ssh/id_lab -p 2222 root@localhost "./benchmarks/pc-mm pointerchasing $size $mode" >"$WORKLOAD_OUT" 2>&1

    sudo kill -INT $PERF_PID
    sleep 3

    echo "      Generating human-readable report..."
    sudo perf mem report -i "$DATA_FILE" --stdio >"$REPORT_FILE"

    sudo rm "$DATA_FILE"
  done
done

echo ""
echo "======================================================"
echo " Master Suite Complete!"
echo " All text reports successfully saved to: $OUT_DIR"
echo "======================================================"
