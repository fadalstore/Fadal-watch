# FadalOS Phase 2 — Work Plan

**Status:** P2.1, P2.2, and P2.3 complete; P2.4 is next
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

### P2.4 — First selected kernel-runtime milestone

Implement only the first bounded item authorized by P2.3: extend the existing 32-bit non-PAE identity map and E820-backed allocator window from 16 MiB to at most 64 MiB. Keep the BIOS disk image, syscall 1–34 ABI, and current smoke trace intact. Require QEMU TCG boots with 32 MiB and 64 MiB RAM plus a serial trace proving an eligible page above 16 MiB can be allocated/touched/freed/reused and that out-of-window or reserved pages are rejected. Keep PAE, x86-64 BIOS entry, AP startup, and broad driver expansion out of scope.

### P2.5 — Phase 2 release checkpoint

After the first selected kernel-runtime milestone, run the full validator and the relevant QEMU/OVMF boot matrix, update both the user-facing build instructions and AI handoff, inspect release hashes, and publish a separate versioned Phase 2 artifact only if it boots and matches the documented configuration. Keep the Phase 1 UTM ZIP frozen and keep BIOS and UEFI images separate unless the architecture decision authorizes otherwise.

## Immediate work queue

1. P2.1 is complete: the UEFI `history` output and exact-order QEMU regression are in place.
2. P2.2 is complete: the window-controls test is state-synchronized and retry-bounded, and persistent-home guest/host checks use the same disk image.
3. P2.3 is complete: ADR 0001 keeps the i386 BIOS runtime and defers a separate x86-64 BIOS path.
4. P2.4 is next: implement only the 16-to-64 MiB non-PAE memory-window proof selected by the ADR, preserving the BIOS ABI and smoke-test contract.

## Out of scope for this plan

This plan does not promise a complete userspace desktop, HTTPS/TLS, multi-user security, persistent command history, a unified BIOS/UEFI kernel, or SMP support. Those require separate designs and testable contracts.
