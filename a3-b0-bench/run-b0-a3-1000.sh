#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
IMAGE=${A3_IMAGE:-capslock-a3}
ITERATIONS=${B0_A3_ITERATIONS:-1000}
TRIALS=${B0_A3_TRIALS:-5}

run_one() {
    mode=$1
    trial=$2
    shift 2

    wall_start=$(python3 -c 'import time; print(time.monotonic_ns())')
    guest_output=$(docker run --rm \
        --volume "$ROOT:/work:ro" \
        --workdir /work/a3-b0-bench \
        "$IMAGE" "$@" "$ITERATIONS")
    wall_end=$(python3 -c 'import time; print(time.monotonic_ns())')
    wall_ns=$((wall_end - wall_start))

    printf 'mode=%s trial=%s wall_ns=%s %s\n' \
        "$mode" "$trial" "$wall_ns" "$guest_output"
}

trial=1
while [ "$trial" -le "$TRIALS" ]; do
    run_one native "$trial" \
        ./target/release/capslock-a3-bench
    run_one native-unoptimised "$trial" \
        ./target/debug/capslock-a3-bench
    run_one stock-qemu "$trial" \
        ./tools/qemu-riscv64-stock \
        ./target/riscv64gc-unknown-linux-gnu/debug/capslock-a3-bench
    run_one capslock-noop "$trial" \
        ./tools/qemu-riscv64-noop \
        ./target-capslock/riscv64gc-unknown-linux-gnu/debug/capslock-a3-bench
    run_one capslock-full "$trial" \
        /capslock-tools/qemu/installation/bin/qemu-riscv64 \
        ./target-capslock/riscv64gc-unknown-linux-gnu/debug/capslock-a3-bench
    trial=$((trial + 1))
done
