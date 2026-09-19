---
name: Kernel descriptor-table assembly
description: Compiler contract for descriptor-table instructions in the freestanding kernel.
---

Descriptor-table inline assembly must declare its memory effects. An `lidt` wrapper needs a `memory` clobber because the descriptor table is populated in C but consumed by the CPU through assembly; without that clobber, optimization can remove the table writes and leave interrupt delivery with invalid gates.

**Why:** The optimized freestanding build removed the otherwise apparently-unused IDT storage, causing the first enabled timer interrupt to raise a general-protection fault.

**How to apply:** Treat IDT/GDT/TSS setup as a compiler-visible memory boundary. Keep the descriptor storage referenced by the assembly and use explicit memory clobbers or equivalent constraints for `lidt`, `lgdt`, and related low-level operations.