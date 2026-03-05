#!/bin/bash
#
# Run all polybench binaries scheduled across all available cores.
# macOS compatible (uses sysctl instead of nproc).
# Usage: ./util/run_polybench_parallel.sh [--enable-lvp] [--enable-comp-simp] [--disable-fdp]
#        (from gem5 root)
#
# Extra flags are forwarded to the gem5 config script for every run.

set -e

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Run all polybench binaries in parallel using gem5 with the Neoverse V2 config.
Any flags are forwarded to fdp_neoverse_v2_binary.py for every benchmark.

Options (forwarded to gem5 config):
  --disable-fdp        Disable FDP for baseline comparison
  --enable-lvp         Enable Load Value Prediction
  --enable-comp-simp   Enable Computation Simplification

Script options:
  -j, --jobs N         Max parallel jobs (default: all available cores)
  -h, --help           Show this help message

Examples:
  $(basename "$0")                              # baseline run
  $(basename "$0") --enable-lvp                 # run with LVP
  $(basename "$0") --disable-fdp                # run without FDP
  $(basename "$0") --enable-lvp --enable-comp-simp  # combine flags
  $(basename "$0") -j 4 --enable-lvp           # limit to 4 parallel jobs
EOF
    exit 0
}

# Parse script-specific args, collect the rest for gem5
CUSTOM_JOBS=""
GEM5_ARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help) usage ;;
        -j|--jobs) CUSTOM_JOBS="$2"; shift 2 ;;
        *) GEM5_ARGS+=("$1"); shift ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

GEM5=$ROOT_DIR/build/ARM/gem5.opt
CONFIG=$ROOT_DIR/configs/example/arm/fdp_neoverse_v2_binary.py
BINARY_DIR=$ROOT_DIR/polybench_binaries
EXTRA_ARGS="${GEM5_ARGS[*]}"

# macOS-compatible core count
if [ -n "$CUSTOM_JOBS" ]; then
    MAX_JOBS=$CUSTOM_JOBS
elif command -v sysctl &>/dev/null; then
    MAX_JOBS=$(sysctl -n hw.ncpu)
else
    MAX_JOBS=$(nproc)
fi

# Build a suffix from the flags for the output directory
if [ -z "$EXTRA_ARGS" ]; then
    SUFFIX="base"
else
    SUFFIX=$(echo "$EXTRA_ARGS" | sed 's/--//g; s/ /_/g')
fi

BENCHMARKS=($(ls "$BINARY_DIR"))
OUTDIR="benchmark_out_${SUFFIX}"

echo "Found ${#BENCHMARKS[@]} benchmarks"
echo "Scheduling across $MAX_JOBS cores"
echo "Extra args: ${EXTRA_ARGS:-none}"
echo "Output dir: $OUTDIR/"
echo "============================================"

parallel_pids=()
failed=0

run_bench() {
    local bench=$1
    local outdir="$OUTDIR/$bench"

    mkdir -p "$outdir"
    echo "[START] $bench"
    if $GEM5 -d "$outdir" $CONFIG --binary "$BINARY_DIR/$bench" $EXTRA_ARGS \
        > "$outdir/stdout.log" 2>&1; then
        echo "[DONE]  $bench"
    else
        echo "[FAIL]  $bench (see $outdir/stdout.log)"
        return 1
    fi
}

for bench in "${BENCHMARKS[@]}"; do
    run_bench "$bench" &
    parallel_pids+=($!)

    # If we've hit the max, wait for one to finish before launching another
    while (( ${#parallel_pids[@]} >= MAX_JOBS )); do
        new_pids=()
        for pid in "${parallel_pids[@]}"; do
            if kill -0 "$pid" 2>/dev/null; then
                new_pids+=("$pid")
            fi
        done
        parallel_pids=("${new_pids[@]}")
        if (( ${#parallel_pids[@]} >= MAX_JOBS )); then
            sleep 1
        fi
    done
done

# Wait for all remaining runs
for pid in "${parallel_pids[@]}"; do
    wait "$pid" || ((failed++))
done

echo ""
echo "=== All runs complete ==="
echo "Results in: $OUTDIR/"
if (( failed > 0 )); then
    echo "WARNING: $failed benchmark(s) failed"
    exit 1
fi
