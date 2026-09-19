---
name: Kernel runtime roadmap
description: Durable sequencing decision for expanding the independent Fadal kernel.
---

Keep the first independent runtime milestone in the existing 32-bit BIOS/protected-mode foundation while adding owned interrupts, timing, memory accounting, and a stable syscall entry point. Move to long mode only after those contracts are exercised by a bootable test.

**Why:** A staged bootable kernel gives each low-level contract a visible smoke-test signal and avoids mixing a paging-mode migration with new runtime behavior.

**How to apply:** Prefer small QEMU-verifiable layers—timer/IRQ, memory map and allocator, syscall ABI, then userspace/filesystem/drivers—while preserving the freestanding/no-host-OS boundary.