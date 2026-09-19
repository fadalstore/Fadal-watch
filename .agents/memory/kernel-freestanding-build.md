---
name: Fadal freestanding build constraints
description: Compiler and boot constraints for the independent Fadal kernel.
---

Keep the freestanding 32-bit kernel build free of implicit SSE/MMX
instructions until the kernel explicitly initializes the processor's floating
point/SIMD state.

**Why:** GCC can auto-vectorize ordinary memory loops into SSE instructions.
The early kernel has no SSE setup, so those instructions can trigger an
exception before serial output or paging diagnostics appear.

**How to apply:** Preserve the kernel's no-SSE/no-MMX/no-vectorization compiler
flags while the early boot path is still 32-bit and freestanding. If SIMD is
needed later, initialize and validate the CPU state as an explicit architecture
milestone rather than enabling it through compiler defaults.