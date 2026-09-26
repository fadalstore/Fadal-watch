# FadalOS Architecture

FadalOS is a freestanding operating-system project. Its target runtime does not boot Linux, use Ubuntu libraries, or depend on a host userspace after the boot handoff. Host tools such as GCC, GNU binutils, QEMU, and GNU-EFI are development tools only.

## System layers

The project has two boot paths that share the same design goal but are built separately.

| Layer | Location | Responsibility |
| --- | --- | --- |
| BIOS boot path | `kernel/boot.S` | Enters protected mode and loads the 32-bit kernel from the raw image. |
| 32-bit kernel | `kernel/kernel.c` | Owns interrupts, memory, processes, syscalls, TTY, VFS, and boot self-tests. |
| Storage | `kernel/fat12.c`, `kernel/ata.c`, `kernel/vfs.c`, `kernel/ramfs.c` | Provides FAT12, ATA PIO, RAMFS, and filesystem-independent lookup. |
| Network | `kernel/rtl8139.c`, `kernel/fnet.c`, `kernel/gitpack.c` | Provides the RTL8139 path, Ethernet/ARP/IPv4 foundations, sockets, and Git protocol groundwork. |
| User bootstrap | `kernel/fsh.c` | Builds `FSH.BIN`, the first ring-3 shell executable. |
| UEFI loader | `kernel/uefi/boot.c` | Discovers GOP, loads `FADAL.KRN`, and transfers control to the 64-bit payload. |
| UEFI desktop | `kernel/uefi/payload.c` | Renders the native GOP desktop and handles serial, keyboard, and PS/2 mouse input. |
| UTM packaging | `utm/`, `tools/create-uefi-image.sh` | Packages the UEFI image for UTM and UTM SE. |
| Termux integration | `scripts/install-fadalwatch-termux.sh`, `scripts/fadalwatch-termux` | Installs the BIOS image and provides Android-side build/run commands. |

## BIOS boot flow

`boot.S` loads the kernel image into memory and enters 32-bit protected mode. The kernel initializes the GDT, IDT, PIC, PIT, E820 memory map, page allocator, heap, slab caches, ATA/FAT12 storage, VFS, network probe, process table, scheduler, syscall gate, and ring-3 shell. Boot self-tests write deterministic traces to COM1. The `kernel/Makefile` smoke test treats those traces as the compatibility contract.

## Userspace boundary

`fsh.c` is compiled as a separate fixed-address executable. The kernel places the resulting `FSH.BIN` in the FAT12 image, loads it through VFS, maps its user pages, and enters it through the ring-3 ABI. Syscall numbers and register meanings are implemented in `kernel/kernel.c`; any ABI change must update both the dispatcher and `fsh.c`.

## UEFI desktop flow

`kernel/uefi/boot.c` runs as a GNU-EFI application. It locates GOP, reads `FADAL.KRN` from the EFI System Partition, and passes a framebuffer descriptor to `fadal64_entry`. `payload.c` is freestanding x86_64 code. It owns its drawing primitives, serial console, PS/2 mouse polling, bounded event queue, Start menu, and draggable terminal window.

The current graphical terminal is a desktop shell surface and not yet a full text-console compositor. Do not assume that the BIOS shell and UEFI payload share memory, syscall, or filesystem code. They are separate deliverables. The loader passes a small `fadal_uefi_services` ABI from `kernel/uefi/net.h` into the payload. Its HTTP callback uses the firmware's EFI HTTP protocol or HTTP Service Binding child and writes bounded response bodies to the ESP. The BIOS RTL8139 stack is intentionally not linked into the UEFI payload; this is a firmware-native backend with explicit capability detection.

## Invariants for changes

A kernel change must preserve freestanding compilation and the independence guard. A syscall change must preserve existing numbers and add new numbers at the end of the ABI. A filesystem change must keep FAT12 mount and read-back tests passing. A UEFI change must preserve the 32-byte FDL64 header and the GOP handoff structure. A packaging change must keep UTM configuration version 4 and the x86_64 emulation profile.

## References

[1]: https://github.com/fadalstore/Fadal-watch "FadalWatch source repository"
[2]: https://www.gnu.org/software/efi/ "GNU-EFI project"
