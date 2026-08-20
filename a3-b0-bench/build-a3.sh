#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
IMAGE=${A3_IMAGE:-capslock-a3}

docker run --rm \
    --volume "$ROOT:/work" \
    --workdir /work/a3-b0-bench \
    "$IMAGE" bash -lc '
        set -eu
        cargo +nightly build --locked --release
        cargo +nightly build --locked
        cargo +nightly build --locked --target riscv64gc-unknown-linux-gnu
        cargo +capslock build --locked \
            --target riscv64gc-unknown-linux-gnu \
            --target-dir target-capslock
    '
