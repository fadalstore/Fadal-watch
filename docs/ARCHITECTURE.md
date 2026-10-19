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
| UEFI loader | `kernel/uefi/boot.c` | Discovers GOP, loads `FADAL.KRN`, and transfers control to the 64-bit payload; exposes bounded EFI file operations for the persistent home directory. |
| UEFI desktop | `kernel/uefi/payload.c` | Renders the native GOP desktop and handles serial, keyboard, and PS/2 mouse input. |
| UEFI page tables | `kernel/uefi/page_tables.c` | Shares the x86-64 four-level page walker, large-page splitting, NX, and PTE permission updates between loader and self-test. |
| UTM packaging | `utm/`, `tools/create-uefi-image.sh` | Packages the UEFI image for UTM and UTM SE. |
| Termux integration | `scripts/install-fadalwatch-termux.sh`, `scripts/fadalwatch-termux` | Installs the BIOS image and provides Android-side build/run commands. |

## BIOS boot flow

`boot.S` loads the kernel image into memory and enters 32-bit protected mode. The kernel initializes the GDT, IDT, PIC, PIT, E820 memory map, page allocator, heap, slab caches, ATA/FAT12 storage, VFS, network probe, process table, scheduler, syscall gate, and ring-3 shell. Boot self-tests write deterministic traces to COM1. The `kernel/Makefile` smoke test treats those traces as the compatibility contract.

## Userspace boundary

`fsh.c` is compiled as a separate fixed-address executable. The kernel places the resulting `FSH.BIN` in the FAT12 image, loads it through VFS, maps its user pages, and enters it through the ring-3 ABI. Syscall numbers and register meanings are implemented in `kernel/kernel.c`; any ABI change must update both the dispatcher and `fsh.c`.

## UEFI desktop flow

`kernel/uefi/boot.c` runs as a GNU-EFI application. It locates GOP, reads `FADAL.KRN` from the EFI System Partition, and passes a framebuffer descriptor to `fadal64_entry`. `payload.c` is freestanding x86_64 code. It owns its drawing primitives, serial console, PS/2 keyboard/mouse polling, bounded event queue, focus-aware terminal/desktop/Start-menu input routing, and geometry-aware terminal window with drag, edge/corner resize, minimize, maximize/restore, and close controls. Resize preserves the opposite edge for single-edge drags, clamps the window to minimum dimensions and the framebuffer work area, updates the active editor input limit, and redraws the terminal viewport. Scrollback rows track explicit newline boundaries separately from width-induced soft wraps, so retained output can be reflowed when the window width changes; a scrolled view keeps its logical history anchor where the bounded buffer permits. Minimized or closed windows can be restored from the taskbar; Start > Terminal reopens the retained in-memory shell session. PS/2 device commands consume their ACK/RESEND responses before mouse streaming starts, keeping the packet stream aligned. `make -C kernel/uefi test-window-resize` exercises reflow, editor/scrollback behavior, edge/corner geometry, bounds, and restoration under QEMU/OVMF.

The FDL64 v3 payload preserves the fixed 32-byte base header and appends a
40-byte `FPM1` descriptor containing page-aligned text, rodata, data, and BSS
bounds. The linker emits separate `R`, `R-X`, `R`, and `RW` ELF load segments,
so it no longer produces an RWX segment. The loader uses `AllocatePages`,
copies the file into the page-aligned runtime extent, zero-fills the remaining
BSS, preflights each identity-mapped 4 KiB leaf, then enforces `R/NX`, `RX`,
`R/NX`, and `RW/NX` in the active four-level tables. It preserves inherited
parent restrictions and edits only payload leaf entries; it enables NXE and
`CR0.WP`, and a required v3 protection failure stops the handoff. Legacy v2 images
are accepted with an explicit warning but do not get this protection policy.
The isolated QEMU test uses the same page-table walker and verifies both NX
instruction faults and supervisor writes to read-only pages, restoring the
firmware mapping afterward.

The graphical terminal renders console output and keyboard echo into the GOP framebuffer, with a bounded 64-row by 160-character scrollback. Its single-line editor supports Left/Right, Home/End, insertion at the cursor, Backspace, and Delete; input is bounded to the visible prompt row and clipped if resizing reduces that row's capacity. PageUp/PageDown scroll the retained history, while new output follows the live tail by default. The terminal remains a small desktop shell surface rather than a complete userspace desktop environment. Do not assume that the BIOS shell and UEFI payload share memory, syscall, or filesystem code. They are separate deliverables. The loader passes a small `fadal_uefi_services` ABI from `kernel/uefi/net.h` into the payload. HTTP uses the firmware's EFI HTTP protocol or HTTP Service Binding child and writes bounded responses to the ESP. The persistent `/home/root` view is a flat FAT directory at `FADALHOME` on the same ESP; `ls`, `cat`, and `write NAME TEXT` use validated, single-component file callbacks, and boot seeds the default README, motd, and profile only when absent. Files persist across guest restarts, but this is not yet a general VFS or a per-user storage boundary. The BIOS RTL8139 stack is intentionally not linked into the UEFI payload; this is a firmware-native backend with explicit capability detection.

## Invariants for changes

A kernel change must preserve freestanding compilation and the independence guard. A syscall change must preserve existing numbers and add new numbers at the end of the ABI. A filesystem change must keep FAT12 mount and read-back tests passing. A UEFI change must preserve the 32-byte FDL64 base header, v3 `FPM1` manifest, page-aligned allocation, zero-filled BSS, the required RX/RNX/RWNX policy, and the GOP handoff structure. A packaging change must keep UTM configuration version 4 and the x86_64 emulation profile.

## References

[1]: https://github.com/fadalstore/Fadal-watch "FadalWatch source repository"
[2]: https://www.gnu.org/software/efi/ "GNU-EFI project"
