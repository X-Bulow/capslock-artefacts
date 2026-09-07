# CAPSLOCK-Lite: Revised Research and Execution Plan (v2)

Date: 7 September 2026
Supersedes: `CAPSLOCK-LITE_execution_plan.md` (2 September 2026)

---

## 1. What changed since v1

v1 asked: *can CapsLock's revoke-on-use model be implemented as portable compiler
instrumentation plus a native runtime, without a custom ISA?*

BorrowSanitizer has already answered that question at the architectural level —
not for revoke-on-use specifically, but for the class of design. Rebuilding the
same pipeline (rustc retag emission, LLVM instrumentation, shadow metadata
transport, cross-language provenance) would take months and produce engineering
rather than knowledge.

The signal in the discussion is Aditya's observation: **BorrowSanitizer caught two
more bugs than CapsLock, missed the same intra-allocation bugs, and neither tool
touches the majority of the benchmark's bugs.** The frontier is coverage and
semantics, not plumbing.

The research question is therefore restated:

> **Hold the instrumentation constant and vary the policy.** Compare Tree Borrows
> against CapsLock's revoke-on-use on a single, identical event stream —
> measuring the detection and cost difference attributable purely to the semantic
> model — and attempt to close the intra-allocation gap that both models share.

## 2. Feasibility: the reuse seam

BorrowSanitizer is built from four components:

1. an experimental `-Zcodegen-emit-retag` flag in the Rust compiler;
2. an LLVM instrumentation pass that inserts runtime checks;
3. an LLVM "wrapper" sanitizer runtime;
4. a Rust "core" runtime, which is where Tree Borrows permission logic lives.

Two properties matter. Its **no-op mode uses only components 2 and 3**, and the
wrapper is decoupled from the core runtime using **weak symbols**. That is a real
seam between the instrumentation pipeline and the policy implementation. It is
not a designed plugin API, and it should not be assumed to be sufficient — but
components 1–3 are reusable plumbing and component 4 is the policy layer.

`runtime/libcapslock.h` is already the structural analogue of component 4:
`capslock_create`, `capslock_borrow`, `capslock_access`, `capslock_revoke`,
`capslock_shadow_store` / `capslock_shadow_load`, `capslock_mark_type`.

**Hypothesis to falsify in a three-day spike:** libcapslock can be linked as an
alternative component 4. If it holds, the Tree-Borrows-vs-revoke-on-use
comparison becomes an A/B on an identical event stream, and any difference is
attributable to the semantic model alone. If it does not hold, the fallback is
still valuable: consume component 1 (`-Zcodegen-emit-retag`) rather than writing
our own rustc/MIR modification, which removes the largest risk item carried over
from v1 Section 2.

## 3. Scope decision: intra-allocation bugs

Responding to Aditya's request for an explicit statement.

**Intra-allocation bugs are in scope, and are the primary target of M-D.**
Rationale:

1. It is the gap *both* tools share, so closing it is new knowledge rather than a
   reimplementation.
2. The runtime already carries per-node `[base, end)` bounds, so the
   representation exists; what is missing is either the information or the rule.
3. It is precisely where capability granularity matters in hardware, so it serves
   both the software and hardware tracks.

What the Week 1 triage decides is the *mechanism* — model change, recovering
information lost before LLVM, or an instrumentation gap — not whether to do it.

## 4. Immediate plan (approximately six weeks)

### W1 — Miss-cause triage (jointly with Aditya)

Take the benchmark cases missed by CapsLock and BorrowSanitizer and classify each
by cause:

- **(M)** model — the rule does not consider it a violation;
- **(I)** information — the distinction was lost before LLVM;
- **(V)** visibility — the access was never seen (libc, FFI, inline asm);
- **(O)** out of scope — not an aliasing bug at all (logic error, uninitialised
  memory, data race).

This table is the ground truth Aditya proposed, and it settles Section 3 with
evidence rather than a stated position. Expect **(O)** to dominate the "majority
of the benchmark's bugs" figure. If so, that must be stated plainly in the final
report, or readers will read it as a tool deficiency rather than a scope boundary.

**Deliverable:** miss-cause table, one row per missed bug.

### W1–W2 — Reuse spike (parallel with W1)

Build BorrowSanitizer, run its no-op mode, inspect the wrapper-to-core weak-symbol
boundary, enumerate the events the wrapper emits, and map each onto the
libcapslock API.

**Deliverable:** yes/no on the Section 2 hypothesis, plus a gap list. Decides
whether M-C follows the reuse route or the own-pass route.

### W2–W3 — Rule differential, in falsifiable form

The v1 Section 4 comparison, restructured so it can be wrong:

1. classify each rule as equivalent, overlapping-but-different, or unique;
2. for every overlapping and unique rule, using the benchmark that the
   classification predicts one tool detects and the other does not;
3. run both tools; a mismatch means the classification is wrong, so fix it.

**Deliverable:** the classification table plus the set of discriminating programs.
Some new programs may be added to the benchmark.

### W3–W6 — M-C engineering (scope set by the spike)

*Reuse route:* implement revoke-on-use as an alternative core runtime; pass the A6
oracle programs, then the benchmark.

*Own-pass route:* wire `capslock_borrow` / `capslock_access` into the pass, connect
shadow metadata, run the A6 oracles end to end. Either way, close the memory-access
categories recorded as unhandled in B4 — `AtomicRMWInst`, `AtomicCmpXchgInst`,
`memcpy`/`memmove`/`memset` and their element-wise atomic counterparts, `VAArgInst`,
and the masked load/store/gather/scatter intrinsics. M-C needs all of them.

**Gate C:** the A6 oracle programs are detected end to end on native x86-64, and
the 15 RustSec PoCs show no regression relative to the QEMU CapsLock baseline.

## 5. Long-term direction

| Stage | Content | Decision value |
| --- | --- | --- |
| **M-D** | Intra-allocation granularity: narrow subobject bounds at field borrows; measure detections recovered on the benchmark and the cost incurred | The actual contribution |
| **M-E** | Toolchain integration — reduced from months to weeks if `-Zcodegen-emit-retag` is reused | Largest risk item outsourced |
| **M-F** | Native performance decomposition, reusing the A3/B0 methodology (native / no-op / full) | Produces the hot-path table |
| **HW gate** | Only after the hot-path table exists: decide what, if anything, hardware should accelerate | Prevents designing hardware from assumptions |

Native x86-64 remains the near-term target and RISC-V/QEMU remain deferred. This
is now more strongly justified than in v1: BorrowSanitizer runs on x86, and an A/B
comparison requires the same target.

A hardware-performance claim would eventually require a cycle-aware architectural
simulator, RTL/FPGA prototype, or real hardware. QEMU can establish functional
behaviour but cannot predict future hardware performance. That is a separate
project, not this semester's.

## 6. Expected capability loss in the Lite design

Unchanged from v1, and still to be presented as an explicit efficiency/coverage
trade-off rather than as equivalence with the original CapsLock:

- **Inline assembly:** individual accesses inside an `asm` block are not generally
  visible to the pass.
- **Uninstrumented libraries:** accesses inside external binaries are invisible
  unless rebuilt with compatible instrumentation.
- **Optimization interaction:** an invalid operation may be transformed or removed
  before a late pass, while early instrumentation can inhibit useful optimization.
- **Metadata continuity:** native x86 pointers carry no architectural capability
  tag, so provenance can be lost at uninstrumented ABI boundaries.

The W1 triage will quantify how much of the observed miss set falls into the
**(V)** category, which makes this section measured rather than asserted.

