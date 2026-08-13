# CapsLock Progress Update

Date: 12 August 2026

## Summary

I have completed the implementation and functional work for A1, A2, A4, and
A5 on an Apple Silicon Mac. I have now also collected the four A3 timing
results on a separate x86-64 machine so that they are not distorted by an
additional ARM64-to-x86-64 emulation layer.

## Completed work

### A1 — Artifact reproduction

- Reproduced the artifact in a `linux/amd64` Docker environment on the Mac.
- Confirmed that the quick tests produced the expected output and diagnostic.
- Confirmed that CapsLock detected all 15 RustSec PoCs. The jobs killed by the
  artifact's 16-way parallel execution were rerun sequentially.
- Completed a clean image build after limiting LLVM and Rust to two build jobs
  to fit the Docker VM's memory limit.
- The clean build took 5776.11 seconds. This is an emulation-specific build
  time and is not being used as a native x86-64 performance result.
- Documented the ThreadSanitizer limitation: TSan could not initialise under
  the Apple Silicon amd64-emulated virtual address layout.

### A2 — LLVM substitution path

- Traced how CapsLock LLVM is substituted into the Rust build.
- Confirmed that Rust's `src/llvm-project` submodule is not rewritten.
- The setup downloads the pinned upstream LLVM revision, applies
  `setup/patches/llvm.patch`, and installs the resulting CapsLock LLVM build.
- Rust is then configured to use that external installation through
  `/capslock-tools/llvm/installation/bin/llvm-config`.

### A4 — Standalone runtime core

- Implemented and reviewed the standalone C runtime in
  `runtime/libcapslock.c` and `runtime/libcapslock.h`.
- The runtime contains allocation tracking, capability trees, permissions,
  shadow memory, borrow, access, and subtree revocation.
- Direct C tests confirm that a valid nested borrow chain is accepted and that
  an access through a revoked capability is rejected.

### A5 — Node types

- Implemented the REF, RAW, and UNSAFECELL node types.
- Added tests for the different invalidation behaviour of RAW versus REF and
  for the UnsafeCell relaxation.
- The combined A4/A5 standalone runtime test suite passed 9/9 tests.

## A3 timing results

### A3 — Performance decomposition

A fixed workload of 100 compression/decompression iterations was measured in
the following four modes. Each value is the mean of five measurements:

| Variant | Mean wall time | Ratio to native |
| --- | ---: | ---: |
| native x86-64 | 0.014996 s | 1.000x |
| stock `qemu-riscv64` | 0.170234 s | 11.352x |
| CapsLock QEMU, capability operations no-op | 1.456172 s | 97.104x |
| full CapsLock QEMU | 32.230098 s | 2149.246x |

Full checking takes 22.133x the no-op CapsLock time. Relative to the total
full-over-native elapsed-time increase, enabling full checking over the no-op
configuration accounts for 95.526%; the remaining 4.474% includes emulation,
instrumented-guest, and no-op CapsLock-QEMU overhead. Exact host and command
metadata and the five raw samples per variant will be added to the reproducible
experiment record.

## Next step

After completing the remaining A3 reproducibility record, I will proceed to
A6 and validate the hand-written oracle programs against the standalone
runtime.

## Slack message draft

Hi Prof. Prateek, I wanted to share a brief progress update. Since this
morning, I have completed A1, A2, A4, and A5. For A1, I reproduced the
artifact in a `linux/amd64` Docker environment on my Apple Silicon Mac,
verified the quick tests, confirmed CapsLock detections for all 15 RustSec
PoCs, and completed a clean build after limiting the LLVM and Rust build
parallelism to fit the available memory. I also documented that the measured
build time is emulation-specific and that TSan cannot initialise correctly in
this environment. For A2, I traced the LLVM substitution path and confirmed
that the build uses a separately patched and installed LLVM through
`llvm-config`, rather than rewriting Rust's LLVM submodule. For A4 and A5, I
worked through the standalone C runtime, including capability trees,
borrow/access/revoke, REF/RAW/UNSAFECELL node types, and the associated tests;
the combined test suite passes 9/9 tests. I have now run A3 on my other
x86-64 computer using 100 compression/decompression iterations per
measurement and five measurements per configuration. The mean times were
0.014996 s for native x86-64, 0.170234 s for stock QEMU, 1.456172 s for
no-op CapsLock QEMU, and 32.230098 s for full CapsLock QEMU. Full checking
took 22.133x the no-op CapsLock time and accounted for 95.526% of the total
full-over-native elapsed-time increase under this decomposition. I will add
the remaining host, command, and raw-sample metadata to the experiment record,
then move on to A6 and validate the hand-written oracle programs.
