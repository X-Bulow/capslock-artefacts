# A3 benchmark, reused by B0

This directory restores the fixed A3 zlib/DEFLATE decompression benchmark and
changes the B0 default from 100 to 1000 timed iterations.

## What each file does

- `src/main.rs` creates a deterministic 4096-byte input, compresses it before
  timing, verifies one untimed decompression, then times only the requested
  decompression loop. A checksum prevents the result from being ignored and
  must match in all four modes.
- `Cargo.toml` selects the artefact copy of `miniz_oxide` 0.8.0. The Cargo dev
  profile is explicitly unoptimised (`opt-level = 0`), while the release
  profile is explicitly optimised (`opt-level = 3`).
- `Cargo.lock` fixes the only registry dependency, `adler2`, to version 2.0.1.
- `Dockerfile` extends the published `capslock-artefacts` image only with the
  ordinary nightly Rust RISC-V standard library required for mode (b).
- `build-a3.sh` builds four executables: optimised ordinary x86-64,
  unoptimised ordinary x86-64, unoptimised ordinary RISC-V, and unoptimised
  CapsLock-instrumented RISC-V in a separate `target-capslock` directory.
- `tools/qemu-riscv64-stock` runs the ordinary RISC-V executable for mode (b).
- `tools/qemu-riscv64-noop` runs the instrumented executable with capability
  state/checking disabled for mode (c).
- the full CapsLock QEMU already inside the published image runs that exact
  same instrumented executable for mode (d).
- `qemu-noop.patch` records how the no-op QEMU differs from full CapsLock QEMU.
- `run-b0-a3-1000.sh` runs five interleaved trials of all five modes at 1000
  iterations and reports both internal `elapsed_ns` and external `wall_ns`.
  The `native` mode uses `target/release`; the additional
  `native-unoptimised` mode uses `target/debug`. Earlier four-mode output in
  which `native` pointed at `target/debug` must not be treated as the optimised
  native baseline in the revised five-way ratio table.

## Startup versus per-iteration cost

A measurement can be approximated as `T(n) = S + n*C`: `S` is fixed startup
work and `C` is the recurring cost of one workload iteration. Startup includes
container/process creation, ELF loading, QEMU initialisation, initial TCG code
translation, runtime/allocator setup, page faults and cold caches. Full
CapsLock QEMU also contains a statically zero-initialised revocation-node pool
of `65536 * 256` entries (about 1.7 GB); mapping and faulting pages associated
with that structure is per-process work, not a cost that should be attributed
to every decompression.

Increasing the loop from 100 to 1000 leaves `S` roughly fixed while multiplying
the recurring term by ten, so stable ratios are stronger evidence that the
result reflects steady work rather than startup.

The benchmark's `elapsed_ns` excludes compression and one warm-up
decompression. The runner's `wall_ns` covers the whole `docker run`, including
startup. Keeping both makes the timing boundary explicit.

## Build and run

From the repository root:

```sh
docker build -t capslock-a3 -f a3-b0-bench/Dockerfile .
./a3-b0-bench/build-a3.sh
./a3-b0-bench/run-b0-a3-1000.sh | tee results/b0-a3-1000-raw.txt
```

Five full trials are expected to take roughly 25 to 35 minutes on the original
x86-64 timing host. Use `B0_A3_TRIALS=1` only for a preliminary correctness
check, not for the final B0 result. Each trial now emits five lines in this
order: `native`, `native-unoptimised`, `stock-qemu`, `capslock-noop`, and
`capslock-full`.
