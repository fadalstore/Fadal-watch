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
    numbers `1` and `2` return PIT ticks and the current process ID. Syscalls
    `4` through `7` provide bounded `read`, `write`, `open`, and `exec`
    services for the FSH ring-3 bootstrap, while syscall `3` remains `exit`.
12. The kernel owns an eight-slot process table with dynamic PID allocation,
    `READY`/`RUNNING` states, and two registered process records. The first
    process is selected as the current ring-3 process; a preemptive scheduler
    is still a later milestone.
13. Syscall number `3` marks the current process as `EXITED`, clears the
    current PID, and decrements the active-process count. The test process
    calls it after the observation syscalls and spins until scheduling exists.
14. The first FAT12 layer formats a 64-sector in-memory volume, mirrors both
    FAT copies, allocates 12-bit clusters, creates 8.3 root entries, and
    writes `KERNEL.TXT` plus the standalone `FSH.BIN` executable during boot.
    The ATA PIO LBA28 driver persists that volume at LBA 113 in the raw disk
    image and reads it back to verify the FAT12 directory and file bytes.
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
20. `fsh.c` is linked into its own fixed-address `fsh.elf`/`fsh.bin`; the
    kernel stores `FSH.BIN` in FAT12, loads it through VFS at boot, and enters
    its entry point in ring 3. The disk executable exercises `read`, `write`,
    `open`, `exec`, `get_ticks`, `get_pid`, and `exit` through `int 0x80`.
    Kernel handlers validate user buffers and VFS paths before accessing them.
21. Paging now uses named x86 flags: kernel mappings are present/writable but
    supervisor-only, the FSH code page is user-readable, and user stacks are
    user-writable. IDT vector `0x0e` is connected to a page-fault ISR that
    reads `CR2`, reports the CPU error code, and fails closed after an
    unhandled kernel or user fault.
22. Each process now owns a page directory and private page-table storage. The
    kernel mappings are cloned as supervisor mappings, the FSH code page is
    shared read-only, and each process receives a distinct physical user stack
    mapped at the same virtual address. Selecting a process loads its page
    directory into `CR3`; boot verifies that the two initial processes cannot
    share their user stack page.
23. Process exit now tears down dynamically allocated process stacks, resets
    the process descriptor to `UNUSED`, and permits PID-slot reuse. Boot creates
    and exits a probe process, verifies that its page returns to the allocator,
    and then confirms that the slot can be allocated again.
24. Syscall `8` is a cooperative `yield` point. It asks the ready selector for
    the next eligible PID and returns that PID to userspace; `FSH.BIN` calls it
    while retrying an empty TTY read. It deliberately does not claim to switch
    CPU registers yet; the later context-switch implementation will consume
    this established ABI point.
25. Processes now own eight bounded file-descriptor slots. Descriptors `0`,
    `1`, and `2` represent TTY input/output/error, while `open` allocates a
    kernel-file handle from slot `3` onward and syscall `9` (`close`) releases
    it. FSH exercises allocation and release when opening `KERNEL.TXT`.
26. Descriptor-directed I/O is active: syscall `read` and `write` accept the
    descriptor in `EDX`. TTY input uses descriptor `0`, console output uses
    descriptors `1` and `2`, and an opened `KERNEL.TXT` handle reads from a
    bounded kernel-backed file buffer with a per-descriptor offset. FSH's
    `cat KERNEL.TXT` command now opens, reads, writes, and closes the handle
    instead of emitting hardcoded file contents.
27. Syscall `10` (`stat`) validates a `KERNEL.TXT` path and copies its size and
    regular-file type into a userspace metadata structure. FSH exercises the
    ABI during startup and exposes `stat KERNEL.TXT` as a shell command.

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
available; virtual address expansion and demand-zero paging remain future
memory-management milestones.

The TTY input boundary is driven by the kernel's PS/2 IRQ1 handler and supports
scancode-to-ASCII translation, Shift letters, Backspace, Enter, and a bounded
ring buffer. Syscall `read` is now nonblocking: it copies a bounded chunk into
the validated ring-3 buffer when input exists and returns `0xffffffff` as the
EAGAIN result when the queue is empty. `FSH.BIN` is a separate ring-3 disk
executable loaded by the kernel, not a parser linked into `kernel.c`; it
retries EAGAIN without duplicating the prompt, parses commands, and writes
responses. This removes the kernel-side `hlt` from the syscall path so a later
timer scheduler can switch away from a userspace process without inheriting a
blocked kernel continuation.
The headless smoke test seeds one `help` line so the blocking path can be
verified without a physical keyboard; `run-vga` accepts live keyboard input.

The current userspace milestone registers process records with distinct user
stacks, loads FSH from FAT12, selects its ring-3 entry, and exercises syscall
numbers `1` through `7` through `int 0x80`. It proves the privilege boundary,
disk-to-userspace loading, and register-based syscall dispatch. The first
scheduler foundation is also present: process records have explicit `READY`,
`RUNNING`, `BLOCKED`, and `EXITED` states, the PIT periodically selects a
ready PID, and a blocking TTY read records a TTY wait reason that keyboard
input wakes. This is intentionally an intermediate stage: the PIT does not
yet replace the hardware interrupt return frame, so full preemptive register
context switching remains the next scheduler milestone. Address spaces are
isolated at the page-table level.

The initial page-fault handler is intentionally fail-closed. It is the
extension point for demand-zero heap pages, stack growth, and process-specific
address spaces; it does not yet allocate missing pages automatically.

## Design boundary

Fadal Kernel is independent from Linux and Ubuntu. Alpine Linux can later be
used as a development/build environment, but it is not part of the boot path.
Fadal is intended to grow into its own runtime, not boot or embed another
operating system. The next architecture decision is whether to move the
protected-mode foundation to x86_64 long mode or keep it as a 32-bit
compatibility stage while adding paging and a 64-bit execution layer.
