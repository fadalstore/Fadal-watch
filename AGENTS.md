# FadalOS Agent Instructions

Read `AI_HANDOFF.md` before changing code. Read `docs/ARCHITECTURE.md` for subsystem boundaries and `docs/BUILD.md` for reproducible commands.

Preserve FadalOS runtime independence from Linux and Ubuntu. Do not add libc, host headers, or host APIs to freestanding kernel code. Keep existing syscall numbers stable and append new ABI entries. Run `scripts/validate-repo.sh` before committing. For UEFI changes, run a real QEMU or UTM boot test and inspect the framebuffer. Keep changes focused, document known limitations, commit verified work, and push the commit to the configured GitHub repository.

The BIOS smoke trace in `kernel/Makefile` and the 32-byte FDL64 header are compatibility contracts. Do not weaken tests to make a build pass. If a feature exposes a loader, linker, or memory-layout limitation, fix that contract first instead of hiding the failure in the renderer or shell.
