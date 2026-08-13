FROM ubuntu:22.04 AS base

ARG UID=1000
ARG GID=1000
RUN groupadd -g $GID user && useradd -u $UID -g $GID -m user
ENV DEBIAN_FRONTEND=noninteractive

# common build and runtime dependencies

FROM base AS depends

RUN sed -i 's/# deb-src/deb-src/' /etc/apt/sources.list
RUN apt-get update && apt-get install -y curl ninja-build make gcc g++
# RUN rustup install 1.85.1 && rustup default 1.85.1
RUN apt-get install -y gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
RUN apt-get install -y git python3 python3-venv
RUN apt-get build-dep -y qemu
RUN apt-get build-dep -y llvm
RUN apt-get install -y cmake

RUN mkhomedir_helper user

USER user

# build-only dependencies

FROM depends AS setupbase

ADD --chown=user:user setup.py /capslock-tools/setup.py

# QEMU

FROM setupbase AS setupqemu

ADD --chown=user:user patches/qemu.patch /capslock-tools/patches/qemu.patch
RUN cd /capslock-tools && python3 setup.py qemu
ADD --chown=user:user build/qemu.sh /capslock-tools/build/qemu.sh
RUN cd /capslock-tools/build && sh qemu.sh

# LLVM

FROM setupbase AS setupllvm

ADD --chown=user:user patches/llvm.patch /capslock-tools/patches/llvm.patch
RUN cd /capslock-tools && python3 setup.py llvm
ADD --chown=user:user build/llvm.sh /capslock-tools/build/llvm.sh
RUN cd /capslock-tools/build && sh llvm.sh

# Rust (depends on LLVM)

FROM setupllvm AS setuprust

ADD --chown=user:user patches/rust.patch /capslock-tools/patches/rust.patch
ADD --chown=user:user build/rust-config.toml /capslock-tools/build/rust-config.toml
RUN cd /capslock-tools && python3 setup.py rust
ADD --chown=user:user build/rust.sh /capslock-tools/build/rust.sh
RUN cd /capslock-tools/build && sh rust.sh

# Set up the runtime environment

FROM depends AS capslocktools

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh /dev/stdin --default-toolchain none -y
ENV PATH="/home/user/.cargo/bin:${PATH}"

COPY --from=setupqemu /capslock-tools/qemu/installation /capslock-tools/qemu/installation
COPY --from=setupllvm /capslock-tools/llvm/installation /capslock-tools/llvm/installation
COPY --from=setuprust /capslock-tools/rust/build /capslock-tools/rust/build
RUN rustup toolchain link capslock /capslock-tools/rust/build/host/stage2
# somehow it's necessary to link cargo manually
# RUN ln -s /capslock-tools/rust/build/host/stage2-tools-bin/cargo /home/user/.rustup/toolchains/capslock/bin/cargo

# ADD --chown=user:user sourceme.sh /capslock-tools/sourceme.sh

USER root
RUN apt-get install -y parallel
USER user
ARG RUST_TOOLCHAIN=nightly-2024-05-26
RUN rustup install $RUST_TOOLCHAIN && rustup default $RUST_TOOLCHAIN
RUN rustup +$RUST_TOOLCHAIN component add miri rust-src
RUN ln -s /home/user/.rustup/toolchains/$RUST_TOOLCHAIN-x86_64-unknown-linux-gnu \
          /home/user/.rustup/toolchains/nightly-x86_64-unknown-linux-gnu

ADD --chown=user:user bin /capslock-tools/bin

USER root
RUN apt-get install -y python3-matplotlib python3-numpy
USER user

ENV PATH="/capslock-tools/bin:/capslock-tools/qemu/installation/bin:${PATH}" \
    LD_LIBRARY_PATH="/capslock-tools/llvm/installation/lib" \
    RUSTFLAGS="-C target-feature=+crt-static" \
    CARGO_TARGET_RISCV64GC_UNKNOWN_LINUX_GNU_LINKER="riscv64-linux-gnu-gcc"
