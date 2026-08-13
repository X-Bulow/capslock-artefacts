# The artefacts are in the form of patches to open-source
# projects. This script downloads the source codes of those
# projects and applies the patches

import subprocess, sys

PROJECTS = {
        "qemu": ("https://github.com/qemu/qemu.git", "6bb4a8a47a43f35a345f107227fcd6abed59e62c", "patches/qemu.patch"),
        "llvm": ("https://github.com/rust-lang/llvm-project.git", "5399a24c66cb6164cf32280e7d300488c90d5765", "patches/llvm.patch"),
        "rust": ("https://github.com/rust-lang/rust.git", "c69fda7dc664e62f8920a02a4e55d6207b212c24", "patches/rust.patch")
}



name = sys.argv[1]
url, rev_hash, patch_path = PROJECTS[name]
subprocess.run(["mkdir", name])
subprocess.run(["git", "-C", name, "init"])
subprocess.run(["git", "-C", name, "remote", "add", "origin", url])
subprocess.run(["git", "-C", name, "fetch", "--depth=1", "origin", rev_hash])
subprocess.run(["git", "-C", name, "checkout", "FETCH_HEAD"])
subprocess.run(["/bin/sh", "-c", f"patch -p1 < ../{patch_path}"], cwd=name)

