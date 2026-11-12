# FadalOS Phase 2 — Work Plan

**Status:** P2.1 complete; P2.2 is next
**Baseline:** Phase 1 UTM release at commit `1d50d1d`
**Purpose:** define a staged, boot-testable path from the Phase 1 freestanding kernel and UEFI desktop prototype toward a more reliable, inspectable FadalOS runtime.

## Phase 2 principles

FadalOS remains a freestanding operating system. Linux, Ubuntu, GCC, QEMU, GNU-EFI, and similar host tools are build/test infrastructure only; no host kernel API, libc, shell, or userspace component may become a guest runtime dependency. The BIOS kernel and UEFI x86-64 payload remain separate boot paths unless a later architecture decision explicitly unifies them. Existing BIOS boot traces, syscall numbers, FDL64 headers, and UTM v4 profiles are compatibility contracts.

Each milestone must be small enough to build and verify independently. Do not claim a graphical or boot feature complete from compilation alone: boot it under QEMU/OVMF (and inspect the framebuffer when relevant), run repository validation, document known limits, and retain a recoverable commit.

## Milestones

### P2.1 — Read-only shell history command *(complete)*

Add a `history` command to the UEFI shell that displays the retained command ring in oldest-to-newest order. The existing eight-entry bound remains authoritative; printing the list adds no extra allocation and does not modify retained entries beyond the normal recording of the submitted `history` command itself. Empty Enter remains excluded, and the `history` command appears as the newest entry. `help` lists the command; entries remain guest-local and volatile.

**Verified:** `make -C kernel/uefi test-terminal-history` checks empty submissions, navigation/draft restoration, recalled-command editing, wraparound, exact oldest-to-newest output order, and the eight-entry display bound. `scripts/validate-repo.sh --uefi` passed, and the final QEMU/OVMF framebuffer was inspected at `kernel/uefi/out/terminal-history.ppm`.

### P2.2 — Regression-suite reliability *(next)*

Investigate two known test issues without relaxing assertions:

- `test-window-controls` has failed on different Files navigation checks across reruns (Backspace from Downloads and Up returning to the first page). Determine whether the cause is QEMU input timing, focus routing, or a guest defect; add deterministic evidence and rerun the full target.
- `test-persistent-home` passed its guest serial checks through reboot, but the Make target's final host-side `mcopy` check did not find `PERSIST` in its source image. Trace which image the guest modified and which image the host inspects; make both inspect the same artifact and preserve the raw-content assertions.

**Acceptance:** each target passes repeatedly against the intended guest image; the tests still fail on deliberate regressions and keep their existing behavioral/data-integrity coverage.

### P2.3 — BIOS runtime architecture decision

Before choosing a large kernel feature, write a short architecture decision record comparing the two supported directions: continue expanding the 32-bit BIOS runtime (whose identity-mapped memory range is currently bounded) or introduce a separate x86-64 BIOS boot path. Include memory-map/allocator limits, interrupt and syscall contracts, process-context switching, SMP/AP startup prerequisites, boot compatibility, and the cost of maintaining two paths. Keep the existing 32-bit BIOS image bootable while evaluating options.

**Acceptance:** one decision is selected with explicit tradeoffs and a QEMU proof-of-concept boundary; no existing syscall number or BIOS smoke trace is changed as a side effect of the decision.

### P2.4 — First selected kernel-runtime milestone

Implement only the first bounded item authorized by P2.3. Candidate areas are extending usable physical-memory accounting beyond the current 16 MiB identity window, or beginning an isolated x86-64 BIOS entry path. Select one, not both, and define a serial/QEMU acceptance trace before implementation. Keep AP startup and broad driver expansion out of scope until their CPU, memory, and per-CPU scheduler prerequisites are verified.

### P2.5 — Phase 2 release checkpoint

After the first selected kernel-runtime milestone, run the full validator and the relevant QEMU/OVMF boot matrix, update both the user-facing build instructions and AI handoff, inspect release hashes, and publish a separate versioned Phase 2 artifact only if it boots and matches the documented configuration. Keep the Phase 1 UTM ZIP frozen and keep BIOS and UEFI images separate unless the architecture decision authorizes otherwise.

## Immediate work queue

1. P2.1 is complete: the UEFI `history` output and exact-order QEMU regression are in place.
2. Start P2.2 by diagnosing the existing window-controls and persistent-home wrapper failures without weakening their assertions.
3. Repeat those targets against the same test images, then record the stable baseline in `AI_HANDOFF.md`.
4. Only after the test gate is reliable, begin P2.3's BIOS runtime architecture decision.

## Out of scope for this plan

This plan does not promise a complete userspace desktop, HTTPS/TLS, multi-user security, persistent command history, a unified BIOS/UEFI kernel, or SMP support. Those require separate designs and testable contracts.
