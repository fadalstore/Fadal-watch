# FadalOS AI Handoff Guide

This file is the starting point for any future engineer or AI agent continuing FadalOS. Read it before editing code.

## Project identity

FadalOS is an independent freestanding operating-system project owned by `fadalstore/Fadal-watch`. The central requirement is independence from Linux and Ubuntu at runtime. Host Linux tools may compile and test the project, but no host kernel API, libc, or shell implementation may be copied into the guest.

## Current verified baseline

The latest verified Git commit is the repository `main` branch. The BIOS path builds and boots under QEMU. The UEFI path builds and boots through GOP at 1280x800. The graphical UEFI desktop contains a workspace, top bar, terminal panel, taskbar, PS/2 cursor, Start menu, unified keyboard/mouse event queue, and draggable terminal window.

The BIOS kernel currently includes preemptive scheduling foundations, dynamic processes, per-process address spaces, paging flags, page-fault handling, heap and slab allocation, FAT12 write/mount support, ATA PIO persistence, VFS, RAMFS, descriptor-backed file I/O, ring-3 `FSH.BIN`, a syscall ABI, TTY input, structured logging, a RAM disk, CPUID/SMP probes, synchronization primitives, an RTL8139 driver, Ethernet/ARP/IPv4 foundations, UDP/TCP prototypes, and Git protocol groundwork.

## First commands

```sh
git clone https://github.com/fadalstore/Fadal-watch.git
cd Fadal-watch
scripts/validate-repo.sh
```

On Termux, use the dedicated installer and launcher:

```sh
pkg update
pkg install curl coreutils
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch-termux.sh | bash
fadalwatch tools
fadalwatch build check
```

## Source map

`kernel/kernel.c` is the BIOS kernel integration point. It contains initialization, the syscall dispatcher, process lifecycle, scheduler hooks, TTY behavior, and the boot trace. `kernel/fsh.c` is the standalone ring-3 shell. `kernel/vfs.c`, `kernel/fat12.c`, `kernel/ramfs.c`, and `kernel/ata.c` implement storage layers. `kernel/fnet.c`, `kernel/rtl8139.c`, and `kernel/gitpack.c` implement the current network path. `kernel/uefi/boot.c` is the UEFI loader. `kernel/uefi/payload.c` is the 64-bit graphical payload. `kernel/uefi/payload.ld` defines the flat payload layout. `tools/patch-fadal64-header.py` patches the FDL64 image header.

## Required workflow

Before a change, read the relevant source and its header. Make the smallest coherent change. Build the affected target. Run `scripts/validate-repo.sh` or the narrower target. Inspect `git diff --check`. Commit a focused change. Push it to GitHub after verification so the next session has a recoverable checkpoint.

Never overwrite a stable artifact with an untested one. Never claim a UEFI graphical feature is complete from compilation alone; boot it under QEMU or UTM and inspect the framebuffer. Never modify the syscall numbers already used by `FSH.BIN`; append new ABI entries instead.

## Compatibility contracts

The BIOS smoke test in `kernel/Makefile` is a contract. Its required serial lines must remain present unless the test is intentionally updated with a documented behavior change. The FDL64 header is 32 bytes and contains the magic, version, header size, image size, and entry offset. The UEFI loader validates those fields before handoff. The UTM configuration uses emulation for x86_64 and must remain compatible with UTM SE configuration version 4.

## Current next milestones

The next safe graphical milestone is a framebuffer terminal text compositor. It must first formalize the UEFI payload memory contract for writable data and zero-initialized storage. The previous experimental terminal buffer work exposed a loader/runtime-memory fault and was intentionally not committed. Fix the payload linker and loader allocation contract before adding a large renderer buffer.

After that, implement keyboard focus, terminal input routing, window controls, and a real event-driven desktop shell. Separately, continue native Git smart-HTTP only after the TCP receive, HTTP parsing, TLS boundary, and certificate strategy are explicitly designed. Do not describe the current `github` shell bridge as a native clone implementation.

## What not to assume

The UEFI payload and BIOS kernel do not share C globals or syscall implementations. The UEFI desktop is not yet a complete userspace desktop environment. The native network stack is not yet a production HTTPS client. The GitHub bridge is a safe command/status bridge, not a claim that the guest can clone over HTTPS. The current scheduler and SMP support are staged implementations with explicit single-BSP guards.

## Definition of done for a milestone

A milestone is complete only when the source is readable, the relevant build passes, the runtime behavior is boot-tested, the documentation is updated, the repository is clean, and the commit is pushed. Include the exact test command and artifact path in the handoff message.

## References

[1]: https://github.com/fadalstore/Fadal-watch "FadalWatch source repository"
[2]: https://github.com/termux/termux-packages "Termux package definitions and build environment"
