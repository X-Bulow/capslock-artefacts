# CAPSLOCK-Lite: Revised Research and Execution Plan

Date: 2 September 2026

## 1. Decisions for the next stage

This revision makes two decisions explicit:

1. **Use native x86-64 first.** The near-term path is LLVM IR to an ordinary native x86-64 binary. RISC-V and QEMU experiments are deferred until the software design works end to end.
2. **Recover Rust semantics from rustc/MIR.** An LLVM pass alone cannot infer all reference, raw-pointer, mutability, `UnsafeCell`, FFI, and borrow-tree events after those distinctions have been lowered away.


The next executable target is therefore:

```text
Rust source
   |
   |  minimally modified rustc/MIR preserves aliasing events
   v
ordinary LLVM IR containing CAPSLOCK-Lite semantic intrinsics/calls
   |
   |  out-of-tree LLVM pass instruments access and metadata propagation
   v
ordinary native x86-64 machine code + native CAPSLOCK-Lite runtime
```

No custom ISA and no QEMU are required on this near-term path.

## 2. Overall research goal

The software goal is to determine whether CapsLock's revoke-on-use model can be implemented as portable compiler instrumentation plus a native runtime, without custom RISC-V instructions, LLVM frame-lowering changes, or modified-QEMU enforcement.

The intended contribution is not merely another LLVM plugin. It is an end-to-end study of:

- Which Rust aliasing-model information must be preserved before LLVM.
- How revoke-on-use checks can be executed efficiently in a native runtime.
- Which protections are preserved or lost when checks move from final machine-level execution to LLVM IR;
- Whether the resulting performance/coverage point is useful relative to the original CapsLock and BorrowSanitizer.

A completely stock Rust compiler is **not** assumed to be sufficient.
A small, well-defined rustc/MIR modification is part of the proposed design.
“Lite” means removing the custom ISA and emulator from the checking path, not removing all Rust compiler changes.

## 3. BorrowSanitizer as a design reference

[BorrowSanitizer](https://borrowsanitizer.com/intro.html) independently supports the same architectural lesson: an LLVM sanitizer can check Rust/C/C++ aliasing dynamically, but rustc must first preserve Rust-specific retag information.

Its broad flow is:

1. rustc emits retag operations when references are created and at relevant function boundaries;
2. an LLVM pass instruments retags, pointer/provenance flow, allocations, deallocations, and memory accesses;
3. shadow heap/stack structures carry provenance separately from pointer addresses;
4. a runtime updates per-allocation Tree Borrows permission trees;
5. matching Clang/LLVM instrumentation follows metadata across Rust/C/C++.

BorrowSanitizer targets Tree Borrows, whereas CAPSLOCK-Lite targets CapsLock's revoke-on-use capability semantics. We should reuse architectural lessons and testing methodology, not silently change our semantic model to Tree Borrows.

But the most importantly, I think we should clarify the difference between these two models, otherwise we will do repetitive work.

Useful design references:

- [retag and compiler integration](https://borrowsanitizer.com/status/january_2026.html);
- [shadow stack and metadata transport](https://borrowsanitizer.com/status/april_2026.html);
- [four-component architecture and no-op mode](https://borrowsanitizer.com/status/may_2026.html);
- [wildcard provenance and metadata garbage collection](https://borrowsanitizer.com/status/june_2026.html).

## 4. Immediate next actions

Before extending the implementation, first determine whether BorrowSanitizer and CapsLock enforce the same rules and, where they differ, what CAPSLOCK-Lite contributes.

Make a comparison from the BorrowSanitizer design, the CapsLock paper, and the original CapsLock repository. Compare:
- the semantic model (Tree Borrows versus revoke-on-use);
- compiler-generated events and the information preserved from MIR;
- pointer provenance and shadow metadata;
- borrow, access, invalidation, and revocation rules; and
- treatment of raw pointers, `UnsafeCell`, FFI, inline assembly, and uninstrumented code.
- Classify each rule as equivalent, overlapping but different, or unique to one system. Do not assume that similar compiler/runtime architectures imply identical semantics.


## 5. Expected capability loss in the Lite design

Moving from final machine-level enforcement to LLVM IR improves execution speed, but it weakens visibility:

- **Inline assembly:** the pass cannot generally see individual memory accesses hidden inside an `asm` block.
- **Uninstrumented libraries:** accesses inside external binaries are invisible unless they are rebuilt with compatible instrumentation.
- **Optimization interaction:** an invalid operation could be transformed or removed before a late pass, while early instrumentation can inhibit useful optimization.
- **Metadata continuity:** native x86 pointers have no architectural capability tag, so provenance can be lost at uninstrumented ABI boundaries or through unsupported operations.

The final report must present this as an explicit efficiency/coverage trade-off
rather than implying full equivalence with original CapsLock.

## 6. Long-term direction (Just an Opinion)

CAPSLOCK-Lite should first become a correct native software baseline. Only then should the project decide which operations deserve hardware support. Otherwise, hardware design would be based on assumptions rather than measured hot paths.

A possible longer-term hybrid design could use hardware for:

- Carrying a small provenance/tag with pointers across all machine accesses.
- Fast validity and bounds lookup.
- caching shadow metadata.
- exposing accesses hidden from LLVM, including inline assembly and
  uninstrumented components.
- efficiently notifying software when an access requires a non-trivial tree transition.

Complex borrow-tree mutation and diagnostics may remain in software. This would make CAPSLOCK-Lite a profiling and semantic reference implementation for hardware/software co-design, rather than a competing dead-end implementation.

QEMU can establish functional behavior but cannot predict the performance of
future hardware. A hardware-performance claim would eventually require a
cycle-aware architectural simulator, RTL/FPGA prototype, or real hardware
implementation.

## 7. Questions

1. Is the intended endpoint an efficient software-only sanitizer, or is the software prototype meant to identify and justify future hardware support?
