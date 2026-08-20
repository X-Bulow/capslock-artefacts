# Local QEMU executables

This directory contains two local, ignored build products used by A3/B0:

- `qemu-riscv64-stock`: unmodified QEMU 8.1.1 built from upstream commit
  `6bb4a8a47a43f35a345f107227fcd6abed59e62c`.
- `qemu-riscv64-noop`: the same QEMU base after the artefact CapsLock patch
  and `../qemu-noop.patch`, with public capability helpers replaced by scalar
  pass-through/no-op implementations.

Expected SHA-256 values:

```text
d8b273f271805f9c1bc53ab2b512d267a76187c27470d245df56c248dc5cd428  qemu-riscv64-stock
c09710d9f4561a848f5a33b17183f910953f00417bd658fe7dd169f2d1b4690e  qemu-riscv64-noop
```
