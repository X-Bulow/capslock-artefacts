# CapsLock M-A progress and professor answers

Date checked: 2026-08-12 (Asia/Singapore)

## Executive status

| Task | Status | Evidence / blocker |
| --- | --- | --- |
| A1 | Complete under amd64 emulation, with limitations documented | The image builds and runs as `linux/amd64`; the quick tests pass and CapsLock detects all 15 PoCs. Clean build: 5776.11 s with two build jobs. TSan cannot initialise under Apple Silicon amd64 emulation. |
| A2 | Complete | The external LLVM path and pinned revision are identified below. |
| A3 | Measurements complete; reproducibility metadata pending | The four variants were measured on the separate x86-64 machine. Each measurement averages five runs of a 100-iteration compression/decompression workload. Exact host details, commands, and the five raw samples still need to be copied into the record. |
| A4 | Complete | 9/9 standalone runtime tests pass. |
| A5 | Complete | REF, RAW, and UNSAFECELL are implemented and tested. |
| A6 | Complete | 9/9 buggy cases flag and 9/9 clean variants pass. |

## A1 - Reproduce the artifact

### Environment and source identity

The artifact was reproduced on an Apple Silicon Mac using Docker Desktop's
`linux/amd64` emulation. Docker reported 10 CPUs and 8,321,515,520 bytes of
memory. The macOS host UID was 501, while the image runs its build scripts as
UID 1000; Docker Desktop's bind-mount mapping kept the files accessible, so
the artifact's native-host UID warning did not block this run. Both the
downloaded reference image and the locally built image were verified as
x86-64 Linux:

```text
docker image inspect: linux/amd64
container uname -m:    x86_64
```

The verification is saved in `results/a1-clean-image-verify.txt`. This is a
functional x86-64 Linux reproduction, but its build and run times are
emulation-specific and must not be compared directly with native x86-64 Linux
measurements.

The artifact does not contain three CapsLock fork commits. Instead,
`setup/setup.py` checks out three upstream base revisions and applies local
patch files without creating new commits:

- qemu: `6bb4a8a47a43f35a345f107227fcd6abed59e62c` + `setup/patches/qemu.patch`
- llvm: `5399a24c66cb6164cf32280e7d300488c90d5765` + `setup/patches/llvm.patch`
- rust: `c69fda7dc664e62f8920a02a4e55d6207b212c24` + `setup/patches/rust.patch`

These pins are at `setup/setup.py:8-10`; checkout and patching happen at
`setup/setup.py:20-22`.

The downloaded reference image was
`corank/capslock-artefacts:latest`, with digest:

```text
sha256:703591b7a949f8653e41cf2ffe9e7a4abe83e01feb54b294f64cdd67f79bfdf8
```

### Quick tests

`docker-test` successfully built and ran `helloworld`, which printed:

```text
Hello, world!
```

The `violation` test produced the expected CapsLock diagnostic:

```text
[CAPSLOCK] Attempting to use an invalid capability for load
```

The diagnostic was emitted before the emulated QEMU process stopped
responding to terminal `Ctrl-C` and the container was stopped externally. This
signal-forwarding/hang behaviour occurred after successful detection and does
not change the quick-test result. Full output is in
`results/a1-docker-test.txt`.

### Existing-bug evaluation

The `docker-eval2` run, together with the sequential CapsLock reruns described
below, produced:

| Tool | Observed result | Interpretation |
| --- | ---: | --- |
| AddressSanitizer | 9 summaries | Matches the nine detected cases in Table 4. |
| ThreadSanitizer | no valid result | The runtime failed before executing the PoC under amd64 emulation; this is not a zero-detection result. |
| Miri | 14 UB lines | The filtered script output contained no UB line for `RUSTSEC-2022-0070`. |
| CapsLock | 15/15 detections | Seven completed in the original parallel run; the eight killed jobs all detected when rerun sequentially. |

Although `docker-eval2 4` was requested, the CapsLock path invokes
`run-all.sh`, which hard-codes GNU Parallel to `-j16`. Running 16 emulated
QEMU processes exceeded the Docker VM's available memory and killed eight
jobs. Those eight were rerun sequentially with separate 60-second limits. The
combined result contains one CapsLock `Attempting` diagnostic for every PoC:

```text
RUSTSEC-2019-0009  RUSTSEC-2019-0023  RUSTSEC-2020-0023
RUSTSEC-2020-0091  RUSTSEC-2021-0031  RUSTSEC-2021-0039
RUSTSEC-2021-0049  RUSTSEC-2021-0053  RUSTSEC-2021-0114
RUSTSEC-2021-0130  RUSTSEC-2022-0002  RUSTSEC-2022-0007
RUSTSEC-2022-0040  RUSTSEC-2022-0070  RUSTSEC-2023-0070
```

Evidence:

- `results/a1-docker-eval2.txt`: original ASan, TSan, Miri, and parallel
  CapsLock run;
- `results/a1-capslock-sequential-rerun.txt`: sequential reruns of the eight
  killed CapsLock jobs;
- `results/a1-capslock-15-of-15.txt`: the combined 15 CapsLock diagnostics;
- `results/a1-capslock-detected-ids.txt`: the 15 unique RustSec IDs.

ThreadSanitizer was probed separately. With Docker's default seccomp profile,
it failed while requesting `personality(ADDR_NO_RANDOMIZE)`. With seccomp
disabled only for the probe container, it still terminated before the PoC ran:

```text
FATAL: ThreadSanitizer: memory layout is incompatible, even though ASLR is disabled.
[probe exit code] 66
```

The supporting logs are `results/a1-tsan-probe-2022-0007.txt`,
`results/a1-tsan-probe-2022-0007-unconfined.txt`, and
`results/a1-tsan-probe-2022-0007-diagnostic.txt`. Consequently, the TSan
column requires a native x86-64 Linux host for a valid comparison.

### Clean build time

The first clean build used the artifact's unrestricted LLVM Ninja parallelism.
Ninja scheduled work for the 10 reported CPUs and exhausted the Docker VM's
approximately 8 GB of memory during LLVM compilation:

```text
c++: fatal error: Killed signal terminated program cc1plus
ResourceExhausted: cannot allocate memory
```

That failed attempt took 1879.34 seconds and is retained only as diagnostic
evidence in `results/a1-clean-build.txt`; it is not the reported clean-build
time.

For the successful clean build, `setup/build/llvm.sh` was limited to
`ninja -j2` and `setup/build/rust.sh` to `--jobs 2`. The image was then built
from scratch with `--no-cache` for `linux/amd64`. It completed and exported as
`capslock-artefacts-clean-j2:latest`:

```text
#40 naming to docker.io/library/capslock-artefacts-clean-j2:latest done
#40 DONE 81.0s
real 5776.11
user 3.31
sys 3.67
```

The clean build therefore took **5776.11 seconds**, or **1 hour 36 minutes
16.11 seconds**. Within the Docker build output, the LLVM stage took 2952.2
seconds and the Rust stage took 2358.9 seconds. The complete successful log is
`results/a1-clean-build-j2.txt`.

### Professor answer

> I reproduced CapsLock in a `linux/amd64` Docker environment on Apple
> Silicon. The pinned QEMU, LLVM, and Rust base revisions are
> `6bb4a8a47a43f35a345f107227fcd6abed59e62c`,
> `5399a24c66cb6164cf32280e7d300488c90d5765`, and
> `c69fda7dc664e62f8920a02a4e55d6207b212c24`, respectively, with the local
> artifact patches applied. The quick tests produced `Hello, world!` and the
> expected invalid-capability report. CapsLock detected all 15 RustSec PoCs;
> eight jobs killed by the artifact's hard-coded 16-way parallel run detected
> successfully when rerun sequentially. A no-cache build completed in
> 5776.11 seconds after limiting LLVM and Rust to two build jobs to fit the
> 8 GB Docker VM. TSan could not be evaluated because its runtime is
> incompatible with the amd64-emulated virtual address layout, so this is
> reported as an environment limitation rather than as zero detections. All
> times are emulation-specific and are not directly comparable to native
> x86-64 Linux measurements.

## A2 - Locate the LLVM substitution

No file repoints Rust's `src/llvm-project` submodule. The artifact bypasses the
submodule and builds a separate patched LLVM tree:

1. `setup/setup.py:9` fetches `rust-lang/llvm-project` at the pinned commit
   `5399a24c66cb6164cf32280e7d300488c90d5765`.
2. `setup/setup.py:22` applies `setup/patches/llvm.patch`, turning that upstream
   source tree into the CapsLock LLVM tree.
3. `setup/build/llvm.sh:2-3` builds the patched tree and installs it inside the
   Docker image at `/capslock-tools/llvm/installation`.
4. The Rust build stage inherits that LLVM installation, while
   `setup/build/rust-config.toml:11` disables Rust's CI LLVM download.
5. `setup/build/rust-config.toml:31,34` point both the RISC-V and x86-64 targets
   to `LLVM_LOCATION/installation/bin/llvm-config`.
6. `setup/build/rust.sh:2` replaces `LLVM_LOCATION` with the separate CapsLock
   LLVM directory and writes `/capslock-tools/rust/config.toml`; line 3 then
   builds the stage-2 Rust compiler against that external LLVM installation.

Professor answer:

> Nothing rewrites the `src/llvm-project` submodule. The artifact bypasses it
> entirely. `setup.py` fetches upstream LLVM at commit
> `5399a24c66cb6164cf32280e7d300488c90d5765` and applies `llvm.patch`.
> `llvm.sh` builds and installs this patched LLVM under
> `/capslock-tools/llvm/installation`. During the Rust build, `rust.sh`
> generates `config.toml` from `rust-config.toml`, disables CI LLVM downloads,
> and sets both target-specific `llvm-config` entries to
> `/capslock-tools/llvm/installation/bin/llvm-config`. Therefore, CapsLock LLVM
> is substituted through Rust's external LLVM configuration, not through
> `.gitmodules`.

## A3 - Decompose overhead

The four variants were measured on the separate x86-64 machine rather than
the ARM64 Mac, avoiding an additional ARM64-to-x86-64 emulation layer. The
fixed workload performed 100 compression/decompression iterations. Each
variant was run five times and the arithmetic mean of those five wall-clock
measurements is reported below.

| Variant | Symbol | Wall time (s) | Ratio to native |
| --- | --- | ---: | ---: |
| native x86-64 | `T_native` | 0.014996 | 1.000x |
| stock qemu-riscv64, unoptimised guest | `T_stock` | 0.170234 | 11.352x |
| capslock-qemu, capability operations no-op | `T_stub` | 1.456172 | 97.104x |
| full capslock-qemu | `T_full` | 32.230098 | 2149.246x |

The incremental factor caused by enabling full capability checking over the
no-op CapsLock configuration is:

```text
T_full / T_stub = 32.230098 / 1.456172 = 22.133x
```

The full run adds 32.215102 seconds over native execution. Of this absolute
slowdown, the portion added between the no-op and full configurations is:

```text
(T_full - T_stub) / (T_full - T_native)
= (32.230098 - 1.456172) / (32.230098 - 0.014996)
= 95.526%
```

The complementary no-op CapsLock/emulation/instrumented-guest portion is:

```text
(T_stub - T_native) / (T_full - T_native) = 4.474%
```

The stock emulator alone is 11.352x native. The no-op CapsLock configuration
is 8.554x the stock-QEMU time, showing that the no-op configuration still has
substantial CapsLock-QEMU/instrumented-guest overhead even without performing
the full checking algorithm. Consequently, the 4.474% complement should not
be described as pure cross-ISA emulation cost.

For a fully reproducible record, still copy from the x86-64 machine: the host
CPU and OS, workload/crate revision and input hash, warm-up policy, exact
commands and build flags, QEMU revisions, and all five raw observations for
each variant. The supplied means and calculations are saved in
`results/a3-timing-summary.txt`.

### Professor answer

> On the x86-64 timing host, using 100 compression/decompression iterations
> per measurement and the mean of five measurements, I obtained 0.014996 s
> for native x86-64, 0.170234 s for stock qemu-riscv64, 1.456172 s for
> CapsLock QEMU with capability operations stubbed to no-ops, and 32.230098 s
> for full CapsLock QEMU. These correspond to 1.000x, 11.352x, 97.104x, and
> 2149.246x native time. Full checking is 22.133x the no-op CapsLock
> configuration. Using the absolute-slowdown decomposition, capability
> checking accounts for 95.526% of the full-over-native slowdown, while the
> no-op CapsLock/emulation/instrumented-guest portion accounts for 4.474%.

## A4 - Runtime core

### Implementation

The standalone implementation is in `runtime/libcapslock.c` with its public API
in `runtime/libcapslock.h`.

It includes an allocation range table, fixed node arena, parent/child/sibling
links, bounds, RW/RO/NA permissions, open-addressed shadow hash map, borrow,
access, and subtree revoke. Access follows the accessing node toward the root
and only processes overlapping sibling subtrees on that path. Invalidated
subtrees are detached.

### Test method and result

A fresh native C11 build and test run was performed on macOS with:

```sh
make -B -C runtime test_runtime
./runtime/test_runtime
```

The build used `-O0 -g -Wall -Wextra -Werror -pedantic`. The combined runtime
suite contains seven A4 core tests and two A5 extension tests; all nine passed.
The complete build and test output is saved in `results/a4-runtime-test.txt`.
An independent manual rerun produced the same `9/9` result; its output is saved
in `results/a4-runtime-test-manual.txt`.

### Professor answer and pasteable output

> Driven only by direct C calls, the standalone runtime accepts a valid nested
> borrow chain and rejects a subsequent access through a revoked descendant.

```text
nested borrow chain: accepted
use-after-revoke: rejected (attempting to use an invalid capability)
[PASS] nested borrow and revoke
9/9 runtime tests passed
```

## A5 - Node type bits

All three node types are implemented: REF, RAW, and UNSAFECELL.

The `RAW versus REF` test creates RAW and REF siblings with identical bounds,
then stores through a third capability derived from their common parent. The
REF subtree becomes NA; RAW remains RW because the store originated under its
parent. A separate test confirms a store below UNSAFECELL does not invalidate
an alias outside the cell subtree.

Pasteable output:

```text
[PASS] RAW versus REF
[PASS] UnsafeCell relaxation
```

## A6 - Hand-written oracle programs

The brief's requested list totals nine cases (3 Listing 1 + Figure 3 + 5
RustSec), despite its final question saying eight. All nine are included:

1. Listing 1(a), use-after-free.
2. Listing 1(b), data race represented by interleaved conflicting accesses.
3. Listing 1(c), unsafe aliasing.
4. Figure 3, raw write followed by stale reference use.
5. RUSTSEC-2020-0023, mutable aliases.
6. RUSTSEC-2022-0002, value reference outliving a guard.
7. RUSTSEC-2021-0114, aliased TLS RNG handles.
8. RUSTSEC-2019-0009, freed backing storage.
9. RUSTSEC-2022-0007, reference retained across an aliasing clear.

Professor answer:

> All nine requested cases flag at their annotated direct-C access line. There
> are zero misses, so there is no algorithm/test-encoding miss to classify.
> All nine clean variants pass, so this test set has zero false positives.

Pasteable summary:

```text
9/9 buggy cases flagged; 9/9 clean variants passed
```

## Verification commands

```sh
cd runtime
make test
```

The same test binaries were also compiled and run with AddressSanitizer and
UndefinedBehaviorSanitizer; both completed without a sanitizer diagnostic.
