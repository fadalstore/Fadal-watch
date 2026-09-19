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
   inside the first 16 MiB identity-mapped window.
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
15. The memory manager provides a page-backed kernel heap with contiguous
    `kmalloc`/`kfree` allocations, zeroed pages, reuse after release, and an
    allocation table for up to 32 live blocks. Boot validates a two-page
    allocation and release cycle.
16. A fixed-size slab allocator provides dedicated caches for eight process
    descriptors and sixteen FAT12 directory entries. Objects are zeroed on
    allocation, returned on release, and validated for reuse during boot.
17. Boot stress testing fills all eight process-cache slots and confirms that
    the ninth allocation fails cleanly, then allocates thirty 256 KiB heap
    blocks, touches both ends of each block, and releases them without a page
    leak. The current slab policy is bounded and fails closed rather than
    expanding its cache automatically.
18. FAT12 now has a mount phase that validates the BPB, boot signature, media
    descriptor, FAT header, and root-directory geometry before scanning root
    entries. Boot mounts an existing volume and verifies `KERNEL.TXT`; only a
    blank or invalid volume takes the first-time format path.
19. The VFS layer defines filesystem-independent mount, root-entry, read-file,
    and write-file operations. FAT12 is the first backend and is mounted as
    the root filesystem through a VFS operation table, leaving room for ext2,
    FadalFS, or another backend without changing kernel callers.

This is the kernel layer, not a complete operating system yet. Filesystem,
process isolation, userspace, drivers, and a native Alpine-compatible
userspace are intentionally staged after the bootable foundation. FAT12 write
support now persists, mounts, and reads the volume through the ATA
primary-master PIO path behind VFS operations; partition discovery and a
general block-device abstraction remain future work.

## Build and run

The build requires a C compiler with 32-bit freestanding support, GNU binutils,
and QEMU:

```sh
make -C kernel
make -C kernel independence
make -C kernel check
make -C kernel run
make -C kernel run-vga
```

`make -C kernel independence` is a build-time contract for Fadal's ownership:
all C translation units use freestanding compilation without host system
headers, the linker uses `-nostdlib`, and the final kernel ELF must have no
unresolved runtime symbols. GCC/binutils are build tools only; BIOS firmware,
ATA hardware, and QEMU are execution targets, not another operating-system
kernel embedded in Fadal.

`make run` boots `kernel/out/fadal-kernel.img` in QEMU, sends serial output to
the terminal, and stops after the smoke-test timeout. The image uses a fixed
112-sector kernel budget for the first boot stage.

Use `make -C kernel run-vga` for the interactive shell: it opens the QEMU VGA
window, where keyboard scancodes are delivered to the PS/2 IRQ1 driver. The
serial output remains attached to the launching terminal. `make run` is the
headless serial-output mode and is useful for logs, but it cannot receive
physical keyboard input while QEMU's display is disabled.

When running with a VGA display, the console accepts keyboard input after the
`Fadal console ready` prompt. Keyboard input is delivered through the kernel's
own IRQ1 handler; no Linux, Ubuntu, or external userspace is involved in the
boot or input path.

The current memory milestone maps physical addresses 0 through 16 MiB and
reserves the lower 1 MiB for firmware/kernel structures. Four page tables
cover the expanded identity-mapped range. The boot sector asks
the BIOS for E820 entries, and only pages reported as usable are released to
Fadal's allocator. If the BIOS does not provide a map, the kernel uses a
conservative 16 MiB fallback and reports that state on the console. This remains
intentionally bounded until an x86_64 paging layer is added.

The kernel heap is currently bounded by the identity-mapped 16 MiB window and
allocates whole contiguous pages. It is suitable for kernel metadata and early
filesystem buffers. Slab allocation for process and FAT12 metadata is now
available; virtual address expansion and demand paging remain future
memory-management milestones.

The interactive shell is driven by the kernel's PS/2 IRQ1 handler, while its
command parser lives in the separately compiled `fsh.c`/`fsh.h` Fadal Shell
module. The kernel exports service callbacks for memory, uptime, status, VFS,
and screen clearing; FSH performs command matching and dispatch. It supports
scancode-to-ASCII translation, Shift letters, Backspace, Enter, a bounded line
buffer, and the commands `help`, `info`, `mem`, `uptime`, `status`, `mount`,
`ls`, `cat KERNEL.TXT`, and `clear`. The current image links FSH as a
freestanding shell module; moving it to a separate ring-3 executable is the
next step after `read`, `write`, and `exec` syscalls are available.

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
