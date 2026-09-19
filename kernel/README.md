# Fadal Kernel

Fadal Kernel is a from-scratch, freestanding kernel foundation. It does not
boot Linux, use Ubuntu, or depend on Alpine userspace. The first milestone
targets the x86 BIOS path so the boot chain is easy to inspect and test:

1. `boot.S` starts in 16-bit real mode and reads the kernel from the disk image.
2. The boot code installs a minimal GDT and enters 32-bit protected mode.
3. `kernel.c` runs without libc or a host operating system.
4. The kernel writes a banner to VGA text memory and to COM1 for QEMU tests.
5. The kernel owns its interrupt path with an IDT, remapped PIC, and keyboard
   IRQ handler.
6. The kernel exposes a small PS/2 console with `help`, `info`, and `clear`
   commands.
7. The kernel enables a bounded 4 MiB identity map and its own page allocator.
8. The kernel programs the PIT at 100 Hz and owns timer IRQ0 for uptime.
9. The kernel reserves an `int 0x80` syscall gate with a ring-3 descriptor so
   future userspace does not need to depend on a host operating system.

This is the kernel layer, not a complete operating system yet. Filesystem,
process isolation, userspace, drivers, and a native Alpine-compatible
userspace are intentionally staged after the bootable foundation.

## Build and run

The build requires a C compiler with 32-bit freestanding support, GNU binutils,
and QEMU:

```sh
make -C kernel
make -C kernel check
make -C kernel run
```

`make run` boots `kernel/out/fadal-kernel.img` in QEMU, sends serial output to
the terminal, and stops after the smoke-test timeout. The image is intentionally
small and uses a fixed 48-sector kernel budget for the first boot stage.

When running with a VGA display, the console accepts keyboard input after the
`Fadal console ready` prompt. Keyboard input is delivered through the kernel's
own IRQ1 handler; no Linux, Ubuntu, or external userspace is involved in the
boot or input path.

The first memory milestone maps physical addresses 0 through 4 MiB and
reserves the lower 1 MiB for firmware/kernel structures. Pages from 1 MiB
through 4 MiB are managed by Fadal's allocator. This is intentionally bounded
until a BIOS memory map and x86_64 paging layer are added.

The console commands are `help`, `info`, `mem`, `uptime`, `status`, and
`clear`. Timer interrupts wake the idle loop and make the uptime signal
independent from the dashboard or any host userspace.

## Design boundary

Fadal Kernel is independent from Linux and Ubuntu. Alpine Linux can later be
used as a development/build environment, but it is not part of the boot path.
Fadal is intended to grow into its own runtime, not boot or embed another
operating system. The next architecture decision is whether to move the
protected-mode foundation to x86_64 long mode or keep it as a 32-bit
compatibility stage while adding paging and a 64-bit execution layer.