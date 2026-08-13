# CapsLock M-A standalone runtime

This directory implements tasks A4-A6 without LLVM or QEMU. It is ordinary
C11 and deliberately starts unoptimised.

## Build and run

```sh
make test
```

`test_runtime` exercises create, borrow, access, revoke, nested borrows,
partial ranges, shadow-memory propagation, REF, RAW, and UNSAFECELL.
`oracle_programs` contains the paper examples and reduced RustSec patterns.

## Design

- An allocation hash table maps address ranges to borrow-tree roots.
- A fixed node arena stores parent, child, sibling, bounds, permission
  (`RW`, `RO`, or `NA`), and type (`REF`, `RAW`, or `UNSAFECELL`).
- An open-addressed shadow hash table maps simulated memory slots to nodes.
- Access starts at the accessing node and walks to the root, invalidating only
  overlapping sibling subtrees along that path. Invalid subtrees are detached,
  so the implementation does not scan the whole tree on every access.
- A store invalidates conflicting `REF` subtrees. A load invalidates conflicting
  mutable (`RW`) `REF` subtrees while preserving shared (`RO`) references.
- A `RAW` node stays valid for accesses through another capability derived
  from its parent, but ordinary references below it are still invalidated.
- A store originating below an `UNSAFECELL` node does not invalidate aliases
  outside that node's subtree.

The paper's Appendix A prints the set-membership cases of `RevPermW` and
`RevPermR` in the opposite order from the Section 4.2 prose. This runtime
follows the prose, as required by the student brief: conflicting capabilities
in the selected set become `NA`.

## A6 case count

The brief asks for three Listing 1 cases, Figure 3, and five RustSec patterns,
which totals nine cases, although its final question says "eight". This
implementation includes all nine plus a clean variant for each:

1. Listing 1(a), use-after-free.
2. Listing 1(b), data race represented by interleaved conflicting accesses.
3. Listing 1(c), overlapping source and destination.
4. Figure 3, a raw-pointer store invalidating a derived reference.
5. RUSTSEC-2020-0023, two mutable aliases from `raw_slice_mut`.
6. RUSTSEC-2022-0002, a value reference outliving its map guard.
7. RUSTSEC-2021-0114, two mutable handles to the same TLS RNG.
8. RUSTSEC-2019-0009, use of freed `SmallVec` backing storage.
9. RUSTSEC-2022-0007, retaining a reference while an alias clears a vector.

The reduced RustSec cases are based on the corresponding programs under
`evaluations/2-existing-bugs/PoCs`.
