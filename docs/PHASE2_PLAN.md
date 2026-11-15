# FadalOS Phase 2 — Work Plan

**Status:** P2.1–P2.5 complete; Phase 2 candidate packaged and verified
**Baseline:** Phase 1 UTM release at commit `1d50d1d`
**Purpose:** define a staged, boot-testable path from the Phase 1 freestanding kernel and UEFI desktop prototype toward a more reliable, inspectable FadalOS runtime.

## Phase 2 principles

FadalOS remains a freestanding operating system. Linux, Ubuntu, GCC, QEMU, GNU-EFI, and similar host tools are build/test infrastructure only; no host kernel API, libc, shell, or userspace component may become a guest runtime dependency. The BIOS kernel and UEFI x86-64 payload remain separate boot paths unless a later architecture decision explicitly unifies them. Existing BIOS boot traces, syscall numbers, FDL64 headers, and UTM v4 profiles are compatibility contracts.

Each milestone must be small enough to build and verify independently. Do not claim a graphical or boot feature complete from compilation alone: boot it under QEMU/OVMF (and inspect the framebuffer when relevant), run repository validation, document known limits, and retain a recoverable commit.

## Milestones

### P2.1 — Read-only shell history command *(complete)*

Add a `history` command to the UEFI shell that displays the retained command ring in oldest-to-newest order. The existing eight-entry bound remains authoritative; printing the list adds no extra allocation and does not modify retained entries beyond the normal recording of the submitted `history` command itself. Empty Enter remains excluded, and the `history` command appears as the newest entry. `help` lists the command; entries remain guest-local and volatile.

**Verified:** `make -C kernel/uefi test-terminal-history` checks empty submissions, navigation/draft restoration, recalled-command editing, wraparound, exact oldest-to-newest output order, and the eight-entry display bound. `scripts/validate-repo.sh --uefi` passed, and the final QEMU/OVMF framebuffer was inspected at `kernel/uefi/out/terminal-history.ppm`.

### P2.2 — Regression-suite reliability *(complete)*

Investigate two known test issues without relaxing assertions:

- `test-window-controls` has failed on different Files navigation checks across reruns (Backspace from Downloads and Up returning to the first page). Determine whether the cause is QEMU input timing, focus routing, or a guest defect; add deterministic evidence and rerun the full target.
- `test-persistent-home` passed its guest serial checks through reboot, but the Make target's final host-side `mcopy` check did not find `PERSIST` in its source image. Trace which image the guest modified and which image the host inspects; make both inspect the same artifact and preserve the raw-content assertions.

**Resolution and verification:** `test-window-controls` now polls for both the Files titlebar and the exact expected selected-row highlight, so an incomplete redraw cannot satisfy navigation. A bounded three-attempt HMP delivery loop recovers occasional lost QEMU arrow-key injections and reports any retry; it does not skip or alter the row, scroll, Enter, Backspace, Home, or End assertions. The full target passed three consecutive post-fix QEMU/OVMF boots; one boot recorded two recovered `Down` deliveries. `test-persistent-home` now runs QEMU against the exact image that Make later inspects with `mcopy` rather than a temporary copy. The guest reboot checks and host-side byte-equality, deleted-file absence, nested deletion, and retained-directory checks all passed against `kernel/uefi/out/persistent-home.img`.

### P2.3 — BIOS runtime architecture decision *(complete)*

Before choosing a large kernel feature, write a short architecture decision record comparing the two supported directions: continue expanding the 32-bit BIOS runtime (whose identity-mapped memory range is currently bounded) or introduce a separate x86-64 BIOS boot path. Include memory-map/allocator limits, interrupt and syscall contracts, process-context switching, SMP/AP startup prerequisites, boot compatibility, and the cost of maintaining two paths. Keep the existing 32-bit BIOS image bootable while evaluating options.

**Decision:** [ADR 0001](adr/0001-bios-runtime-architecture.md) keeps the existing 32-bit BIOS runtime and compatibility contracts, defers a separate x86-64 BIOS runtime, and authorizes only a 16-to-64 MiB non-PAE identity-map/allocator proof for P2.4. The ADR records the memory, interrupt/syscall, process, SMP, boot-compatibility, and maintenance tradeoffs plus exact QEMU acceptance checks. No BIOS syscall number or smoke trace changed as a side effect of the decision.

### P2.4 — First selected kernel-runtime milestone *(complete)*

P2.4 extends the existing 32-bit non-PAE identity map and E820-backed allocator window from 16 MiB to at most 64 MiB. The no-E820 fallback remains bounded at 16 MiB. User code/stack/heap page tables remain private; identical supervisor-only tables above the first 8 MiB are shared to reduce per-process table storage while preserving the full identity map. The BIOS image layout, syscall 1–34 ABI, and legacy smoke trace are unchanged.

**Verified:** `make -C kernel check` passes the original serial smoke contract and runs `test-memory-window` under QEMU TCG at 16 MiB (negative control: no high-page allocation; allocator remains E820-bounded), 32 MiB, and 64 MiB. The 32/64 MiB runs allocate a usable E820 page above 16 MiB, write/read a full-page pattern, free/reuse the same page, and verify low-reserved, E820-unusable when present, and out-of-window bounds. The complete BIOS check also confirms process address-space isolation, private user-page mapping, ring-3 syscall dispatch, and timer context switching remain intact.

### P2.5 — Phase 2 release checkpoint *(complete)*

**Verified:** `scripts/validate-repo.sh --uefi` and the complete UEFI QEMU/OVMF regression matrix passed. The separate `FadalOS-Phase2-UTM-iOS.zip` contains the current UEFI image and the P2.4 BIOS image in distinct UTM profiles. Each staged profile booted under its documented QEMU/OVMF configuration; ZIP integrity and per-image/archive SHA-256 checks passed. The Phase 1 UTM ZIP aliases and their manifest/installer inputs were not changed. See [Phase 2 release record](PHASE2_RELEASE.md) for checksums, test evidence, and known limits.

**Release constraint:** the BIOS payload uses 57,273 of its fixed 57,344-byte (112-sector) loader budget, leaving 71 bytes. Do not add BIOS runtime features until a separately tested change increases that capacity or reduces payload size. BIOS and UEFI remain separate runtimes and artifacts.

## Immediate work queue

1. P2.1 is complete: the UEFI `history` output and exact-order QEMU regression are in place.
2. P2.2 is complete: the window-controls test is state-synchronized and retry-bounded, and persistent-home guest/host checks use the same disk image.
3. P2.3 is complete: ADR 0001 keeps the i386 BIOS runtime and defers a separate x86-64 BIOS path.
4. P2.4 is complete: 16/32/64 MiB QEMU controls validate the bounded non-PAE memory window.
5. P2.5 is complete: the Phase 2 artifact is separate, boot-verified, checksummed, and documented; Phase 1 archives remain frozen.
6. Next proposed milestone: P2.6 should safely increase BIOS loader capacity with an explicit image-size ceiling and QEMU over-limit negative control before further BIOS kernel growth.

## Out of scope for this plan

This plan does not promise a complete userspace desktop, HTTPS/TLS, multi-user security, persistent command history, a unified BIOS/UEFI kernel, or SMP support. Those require separate designs and testable contracts.
