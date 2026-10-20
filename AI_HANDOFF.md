# FadalOS AI Handoff Guide

This file is the starting point for any future engineer or AI agent continuing FadalOS. Read it before editing code.

## Project identity

FadalOS is an independent freestanding operating-system project owned by `fadalstore/Fadal-watch`. The central requirement is independence from Linux and Ubuntu at runtime. Host Linux tools may compile and test the project, but no host kernel API, libc, or shell implementation may be copied into the guest.

## Current verified baseline

The latest verified Git commit is the repository `main` branch. The BIOS path builds and boots under QEMU. The UEFI path builds and boots through GOP at 1280x800. The graphical UEFI desktop contains a workspace, top bar, taskbar, PS/2 cursor, Start menu, unified keyboard/mouse event queue, a draggable/resizable terminal window with minimize/maximize/restore/close controls, and focus-aware keyboard routing.

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

The UEFI payload now has an FDL64 v3 memory contract: the fixed 32-byte base
header is preserved, and a 40-byte `FPM1` descriptor immediately after it
records page-aligned text, rodata, data, and BSS extents. The loader allocates
page-aligned pages, copies the file, zeroes the BSS tail, validates the
descriptor, enables NXE and `CR0.WP`, and installs `R/NX`, `RX`, `R/NX`, and
`RW/NX` permissions before entering the payload. `kernel/uefi/page_tables.c`
is shared by the production loader and the EFI self-test; it walks four-level
identity mappings, splits 1 GiB/2 MiB entries as needed, preserves inherited
parent restrictions, and edits only payload leaf PTEs. FDL64 v2 remains
loadable for compatibility but runs without W^X and produces a warning.

The input-routing milestone now gives the terminal, desktop, and Start menu
explicit focus states. `Tab` toggles terminal/desktop focus, Escape returns
from non-terminal focus, mouse clicks select the terminal or Start menu, and
PS/2 AUX bytes are separated from keyboard bytes. Shell keystrokes are ignored
unless the terminal owns focus; UART input remains an explicit path back to the
terminal.

The UEFI terminal interaction milestone is implemented: Left/Right, Home/End,
insertion at the cursor, Backspace, and Delete edit the current command; PageUp
and PageDown page through the bounded 64-line scrollback. The editor limits the
line to the visible prompt row, and the terminal returns to the live tail when a
new prompt begins.

Verified commands are `scripts/validate-repo.sh --uefi`,
`make -C kernel/uefi test-page-protection`,
`make -C kernel/uefi test-keyboard-focus`,
`make -C kernel/uefi test-terminal-input`,
`make -C kernel/uefi test-window-controls`,
`make -C kernel/uefi test-window-resize`, and
`make -C kernel/uefi test-persistent-home`. The focus regression boots the
production ESP under QEMU/OVMF, proves desktop-focused typing does not reach the
shell, then returns focus and executes `help`. The terminal-input regression
reconstructs commands through cursor editing and verifies that PageUp/PageDown
change and restore the framebuffer view. Inspect
`kernel/uefi/out/keyboard-focus.ppm`,
`kernel/uefi/out/terminal-scrollback.ppm`, and
`kernel/uefi/out/terminal-tail.ppm`, and
`kernel/uefi/out/window-controls.ppm`. The window regression drives the PS/2
mouse and verifies maximize/restore geometry, minimize/taskbar restore, close,
and reopening the retained terminal session from Start. The isolated protection test must report
both the NX instruction-fetch fault (`error=0x11`) and read-only write fault
(`error=0x3`); the linker should show separate `R`, `R E`, `R`, `RW` LOAD
segments with no RWX warning. Terminal resizing is implemented on all four
edges and corners. Opposite edges stay anchored for single-edge drags; the
window enforces minimum dimensions and framebuffer work-area bounds. Scrollback
records whether each boundary is an explicit newline or a soft wrap, then
reflows retained text to the new width while preserving the user's visible
scrollback anchor. The active command editor is clipped to the new row capacity.
The QEMU/OVMF regression verifies repeated shrink/expand cycles, restoration of
long output, command editing during resize, PageUp/PageDown behavior across
reflow, edge/corner drags, minimum dimensions, and restored original bounds. Its
final geometry capture is `kernel/uefi/out/window-resize.ppm`. Keep the BIOS
shell and UEFI desktop as separate deliverables. Always boot-test loader or
compositor changes under QEMU/UTM and inspect the framebuffer.

The persistent-home view is backed by the ESP's `FADALHOME` directory through
validated loader callbacks. `ls`, `cat PATH`, `write PATH TEXT`, `mkdir PATH`,
and `rm PATH` use EFI file operations; first boot seeds README, motd, and profile
files only when missing. Files can live in nested directories created one level
at a time (each parent must already exist); writes replace old content, are
capped at 4096 bytes, and survive a guest reboot on the same disk. `rm` removes
files only, and EFI metadata checks prevent directory deletion or replacement.
The QEMU/OVMF test verifies listing, reading, overwrite truncation, directory
creation, nested file operations, path confinement, reboot persistence, safe
deletion, and exact file bytes extracted from the FAT image. This is not yet a
general VFS: per-user isolation, directory removal, and power-loss journaling
remain unimplemented.

The shell path-resolution slice is implemented with a bounded current-working-
directory resolver initialized at `/home/root`; the live path appears in the
prompt. `pwd`, `cd [PATH]`, `ls [PATH]`, `cat PATH`, `write PATH TEXT`,
`mkdir PATH`, and `rm PATH` accept normalized absolute or relative paths with
`.` and `..`. `/etc` files are read-only; writes and deletes remain confined to
`/home/root`, and `mkdir` requires its parent directory to exist.
The persistent-home regression also verifies `cd /etc`, `pwd`, a relative read of
`motd`, relative `cd` back to `/home/root`, and reading the seeded README there.
It creates a directory and nested file, confirms both survive reboot, then
removes the nested file and verifies the directory is neither removable nor
replaceable by `write`.

Separately, continue native Git smart-HTTP only after the TCP receive, HTTP parsing, TLS boundary, and certificate strategy are explicitly designed. Do not describe the current `github` shell bridge as a native clone implementation.

## What not to assume

The UEFI payload and BIOS kernel do not share C globals or syscall implementations. The UEFI desktop is not yet a complete userspace desktop environment. The native network stack is not yet a production HTTPS client. The GitHub bridge is a safe command/status bridge, not a claim that the guest can clone over HTTPS. The current scheduler and SMP support are staged implementations with explicit single-BSP guards.

## Definition of done for a milestone

A milestone is complete only when the source is readable, the relevant build passes, the runtime behavior is boot-tested, the documentation is updated, the repository is clean, and the commit is pushed. Include the exact test command and artifact path in the handoff message.

## References

[1]: https://github.com/fadalstore/Fadal-watch "FadalWatch source repository"
[2]: https://github.com/termux/termux-packages "Termux package definitions and build environment"
