# ADR 0001: BIOS runtime architecture

- **Status:** Accepted
- **Date:** 2026-11-13
- **Phase:** P2.3
- **Decision owners:** FadalOS project

## Decision

Keep the existing x86 32-bit protected-mode BIOS kernel as the supported BIOS runtime. Do not convert its current boot sector or kernel in place to long mode, and do not treat the x86-64 UEFI payload as a BIOS kernel. Preserve the BIOS image, boot contract, syscall ABI, and smoke-test trace.

Authorize P2.4 to investigate one bounded extension of the existing 32-bit physical-memory window: grow the identity-mapped and allocator-usable range from 16 MiB to at most 64 MiB using ordinary 32-bit, non-PAE 4 KiB paging. This is an initial resource milestone, not a promise of a permanent 32-bit architecture. It must use E820 type-1 ranges, continue reserving the low 1 MiB, and fail safely when E820 or mapped-page constraints do not support an allocation. Do not implement x86-64 BIOS entry, PAE, AP startup, new drivers, or broad scheduler work in P2.4.

## Context and current contracts

The BIOS boot sector starts in 16-bit real mode, gathers up to 32 E820 entries in low memory, reads a fixed 112-sector kernel with legacy INT 13h CHS into `0x10000`, installs a flat GDT, enters 32-bit protected mode, and calls the kernel at that address. The stack is fixed at `0x90000`. The disk layout places the boot sector at LBA 0, the kernel in LBAs 1–112, and the FAT volume at LBA 113. The 512-byte sector and `0xAA55` signature are compatibility requirements. See `kernel/boot.S`, `kernel/linker.ld`, and `kernel/Makefile`.

The current kernel creates a 16 MiB identity map from four page tables, reserves the first 1 MiB, and only offers E820-reported usable pages in the remaining mapped range to its 32-bit physical/identity-pointer allocator. The E820 consumer stores at most 32 records and ignores ranges whose upper base or length words are nonzero. The current memory ceiling therefore limits both usable physical RAM and direct pointer access; expanding only the E820 scan would be unsafe without corresponding mappings and allocator bounds. See `kernel/kernel.c` (`IDENTITY_MAP_BYTES`, paging initialization, E820 filtering, and page allocation).

The BIOS runtime already contains a substantial 32-bit platform contract: flat 32-bit GDT and TSS, 32-bit IDT/ISR frames using `pusha`/`iret`, PIC/PIT/PS/2 interrupt paths, ring-3 entry and address spaces, timer-driven context switching with CR3 reload, and `int 0x80`. Syscalls 1–34 are published in both `kernel/kernel.c` and `kernel/fsh.c`; the ABI uses EAX for the number/result and EBX/ECX/EDX (plus ESI for applicable calls) for arguments. FSH is a separately loaded ring-3 disk executable. Existing required serial lines in `make -C kernel check` are also a contract. None of these may be renumbered or silently changed by the memory-window proof.

The current x86-64 UEFI path is separate, not an alternative BIOS entry point. Its loader is a GNU-EFI PE/COFF application, uses EFI calling conventions and firmware protocols, and invokes a graphical FDL64 payload through a loader-defined framebuffer/service-table interface. It currently continues to use EFI callbacks and does not call `ExitBootServices`; it is not the BIOS kernel's allocator, device, interrupt, syscall, or userspace runtime. The FDL64 v3 page-permission policy and page-table walker are useful references, but the walker uses EFI allocation types and assumes firmware-owned four-level identity mappings, so it is not directly reusable as a BIOS paging subsystem.

## Options considered

### A. Continue the 32-bit BIOS runtime and widen the bounded memory window — selected

This preserves the known boot path and the established CPU, disk, process, interrupt, and userspace contracts. A 64 MiB cap requires a bounded extension of the ordinary 32-bit page-table coverage and allocator bookkeeping, but does not require 64-bit pointers or a new interrupt/syscall ABI. It makes the first architectural pressure point—memory beyond 16 MiB—measurable before deciding whether a full runtime migration is justified.

Costs and limits remain explicit: the address model is 32-bit; this milestone will not address memory above 4 GiB; the kernel has no NX/W^X policy comparable to the UEFI payload; and the current E820 fallback behavior must be reviewed rather than generalized. PAE and long mode remain future architecture work, not implicit follow-ons.

### B. Add a separate x86-64 BIOS boot/runtime — deferred

A separate path is the cleanest route if the BIOS runtime needs a 64-bit address space, memory above the 32-bit physical ceiling, or a native x86-64 process ABI. It must remain parallel initially so the proven i386 image stays bootable. It is not a compiler-flag change: it requires its own BIOS loader and disk-read policy, CPUID long-mode validation, PAE and four-level page tables, EFER/CR0/CR4 transition, 64-bit GDT/IDT/TSS/interrupt frames, syscall and context-switch design, memory/reservation handoff, and a new QEMU matrix. The current 32-bit pointers, saved frames, process page tables, syscall arguments, and device assumptions need an audited port. A second supported runtime also adds ongoing build, boot, and compatibility maintenance.

The UEFI payload does not remove these costs. Reusing it under BIOS would require a BIOS loader and a new framebuffer, memory, and service contract; it would still not provide the BIOS kernel's syscall/process runtime, and its current persistent-home and HTTP callbacks are EFI-backed.

## P2.4 QEMU proof-of-concept boundary

Implement only the 16 MiB-to-64 MiB non-PAE memory-window extension. Keep the current BIOS image and `make -C kernel check` behavior intact. Add a focused serial trace and tests that:

1. boot the existing BIOS path under QEMU TCG with explicit 32 MiB and 64 MiB RAM configurations;
2. prove the E820-aware allocator can allocate, touch, free, and reuse a page above 16 MiB when the firmware reports such usable RAM;
3. prove allocations remain inside mapped, usable E820 pages, never consume the reserved low 1 MiB or pages beyond the configured 64 MiB cap, and fail closed when no eligible page exists; and
4. retain the existing boot smoke lines, image layout, ring-3 entry, timer context-switch trace, and syscall 1–34 behavior.

The proof ends at bounded memory discovery, mapping, and allocator behavior. It does not claim support for RAM beyond 64 MiB or 4 GiB, arbitrary E820 map layouts, PAE, long mode, AP startup, or a general virtual-memory manager. If the proof shows that the 64 MiB cap is insufficient or that a 32-bit mapping model is no longer the right foundation, reopen this ADR before starting an x86-64 BIOS path.

## Consequences

- Existing BIOS users retain the same boot image format, 32-bit entry, disk layout, interrupt conventions, syscalls, and smoke-test contract.
- P2.4 can target one independently testable resource limit without changing the BIOS ABI or adding a second kernel runtime.
- UEFI and BIOS remain separate freestanding boot paths; Linux/Ubuntu and other host tools remain build/test infrastructure only, never FadalOS runtime dependencies.
- The project accepts the 32-bit ceiling and lack of BIOS-kernel NX for this phase. A future x86-64 BIOS decision will require a separate migration plan, not a silent compatibility change.

## References in this repository

- `kernel/boot.S` — BIOS E820 collection, CHS load, and protected-mode entry.
- `kernel/kernel.c` — paging, E820 filtering, physical allocator, interrupt/syscall frames, process address spaces, scheduler, and SMP scaffolding.
- `kernel/interrupts.S` — 32-bit interrupt and syscall entry/return stubs.
- `kernel/fsh.c` — userspace syscall numbers and calling convention.
- `kernel/Makefile` — fixed image layout, freestanding build, and required BIOS smoke trace.
- `kernel/uefi/boot.c`, `kernel/uefi/payload.c`, `kernel/uefi/page_tables.c` — separate GNU-EFI runtime, payload contract, and firmware-mapping protection code.
- `docs/PHASE2_PLAN.md` — Phase 2 milestones and acceptance gates.
