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
7. The kernel reads the BIOS E820 map and uses it to bound its page allocator
   inside the first 4 MiB identity-mapped window.
8. The kernel programs the PIT at 100 Hz and owns timer IRQ0 for uptime.
9. The kernel reserves an `int 0x80` syscall gate with a ring-3 descriptor so
   future userspace does not need to depend on a host operating system.
10. The kernel maps a small ring-3 test process with a private user code page,
    user stack, TSS kernel stack, and a first syscall transition.
11. The syscall ABI passes a `pusha` register frame to the kernel; syscall
    number `1` returns the current PIT tick count in `EAX`, while syscall
    number `2` returns the current test process ID (`1`).
12. The kernel owns an eight-slot process table with dynamic PID allocation,
    `READY`/`RUNNING` states, and two registered process records. The first
    process is selected as the current ring-3 process; a preemptive scheduler
    is still a later milestone.
13. Syscall number `3` marks the current process as `EXITED`, clears the
    current PID, and decrements the active-process count. The test process
    calls it after the observation syscalls and spins until scheduling exists.
14. The first FAT12 layer formats a 64-sector in-memory volume, mirrors both
    FAT copies, allocates 12-bit clusters, creates an 8.3 root entry, and
    writes `KERNEL.TXT` during boot. The ATA PIO LBA28 driver then persists
    that volume at LBA 113 in the raw disk image and reads it back to verify
    the FAT12 directory, cluster chain, and file bytes.

This is the kernel layer, not a complete operating system yet. Filesystem,
process isolation, userspace, drivers, and a native Alpine-compatible
userspace are intentionally staged after the bootable foundation. FAT12 write
support now persists and reads the initial volume through the ATA primary-master
PIO path; partition discovery and a general block-device abstraction remain
future work.

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
reserves the lower 1 MiB for firmware/kernel structures. The boot sector asks
the BIOS for E820 entries, and only pages reported as usable are released to
Fadal's allocator. If the BIOS does not provide a map, the kernel uses a
conservative 4 MiB fallback and reports that state on the console. This remains
intentionally bounded until an x86_64 paging layer is added.

The console commands are `help`, `info`, `mem`, `uptime`, `status`, and
`clear`. Timer interrupts wake the idle loop and make the uptime signal
independent from the dashboard or any host userspace.

The current userspace milestone is intentionally tiny: the kernel registers
two process records with distinct user stacks, selects PID 1, and its test
program puts
syscall numbers `1`, `2`, and `3` in `EAX`, calls `int 0x80`, receives the
PIT tick count and the dynamic current process ID, then marks itself exited.
It proves the privilege boundary and a register-based syscall dispatch path,
but it is not yet a scheduler or general process model.

## Design boundary

Fadal Kernel is independent from Linux and Ubuntu. Alpine Linux can later be
used as a development/build environment, but it is not part of the boot path.
Fadal is intended to grow into its own runtime, not boot or embed another
operating system. The next architecture decision is whether to move the
protected-mode foundation to x86_64 long mode or keep it as a 32-bit
compatibility stage while adding paging and a 64-bit execution layer.
