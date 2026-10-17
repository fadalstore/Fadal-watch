# FadalOS Build Guide

This guide defines the supported build paths. Run commands from the repository root unless a command changes directory explicitly.

## Fast path

On a prepared Linux or Termux environment:

```sh
scripts/bootstrap-fadalwatch.sh
```

The script builds the BIOS image, runs the canonical smoke test, copies the result to `artifacts/fadal-kernel.img`, and writes `artifacts/SHA256SUMS`.

To build without starting QEMU:

```sh
scripts/bootstrap-fadalwatch.sh --no-check
```

To run the complete repository validator:

```sh
scripts/validate-repo.sh
```

## BIOS kernel

The BIOS path is the primary reproducible build.

```sh
make -C kernel clean all
make -C kernel check
make -C kernel run
make -C kernel run-vga
```

The image is written to `kernel/out/fadal-kernel.img`. `check` must remain the required pre-push test because it verifies the serial boot trace, FAT12 persistence, VFS, scheduler, syscalls, network path, and independence guard.

The default compiler variables can be overridden when a toolchain uses different names:

```sh
make -C kernel CC=clang AS=as LD=ld OBJCOPY=objcopy
```

## UEFI payload

The UEFI path requires GNU-EFI development files and an x86_64 linker environment.

```sh
make -C kernel/uefi clean check
make -C kernel/uefi payload
rm -f kernel/out/fadal-uefi.img
make -C kernel/uefi image \
  UEFI_KERNEL="$PWD/kernel/uefi/out/fadal-uefi.krn"
```

The resulting image is `kernel/out/fadal-uefi.img`. The payload ELF now has
separate page-aligned `R`, `R-X`, `R`, and `RW` load segments for metadata,
text, rodata, and data/BSS; the previous RWX linker warning is gone. At runtime,
the EFI loader allocates page-aligned memory and installs `R/NX`, `RX`, `R/NX`,
and `RW/NX` PTE permissions respectively. It enables `EFER.NXE` and `CR0.WP`,
splitting firmware 1 GiB/2 MiB mappings into 4 KiB leaves when necessary. It
preflights the complete payload mapping before changing permissions and refuses
to enter a v3 payload if NX or the required page-table layout is unavailable.

FDL64 v3 preserves the fixed 32-byte base header and its existing fields. A
40-byte `FPM1` segment-layout descriptor follows that header and gives the
page-aligned text, rodata, data, and BSS boundaries; `memory_size` records the
complete runtime extent while the EFI file size records bytes on disk. The
loader reads the file into the full page-aligned allocation, zeroes the rest of
the runtime pages (including BSS), validates the descriptor, and only then
applies permissions and transfers control. The walker preserves inherited
parent permissions and changes only the target 4 KiB leaf, so neighboring
firmware mappings are not made more permissive. Legacy v2 images remain loadable
for compatibility, but are explicitly warned and do not receive W^X protection.
The UEFI desktop terminal keeps a fixed 64-line by 160-character scrollback and
draws console output and keyboard echo into the GOP framebuffer. Keyboard focus
is explicit: `Tab` toggles terminal/desktop focus, and input is suppressed while
the desktop or Start menu owns focus. Mouse AUX bytes are routed separately from
PS/2 keyboard bytes.

### Isolated page-protection test

The test application is a separate EFI image and uses the same
`page_tables.c` walker as the production loader; it does not call Linux/Ubuntu
kernel APIs. Under a private #PF IDT it first sets NX and verifies an
instruction-fetch fault (`CR2` points at the test page; error bits `P=1,
W=0, U=0, I/D=1`), then clears NX and confirms execution works. It next makes
the code page read-only, enables `CR0.WP`, attempts a supervisor write, and
verifies the expected present/write fault (`P=1, W=1, U=0`), unchanged byte,
and continued execution. Fixed assembly recovery entries resume after each
expected fault. The test restores the original PTE path, any split mappings,
`CR0`, and IDT, then frees its test page. The shared helper clears `CR0.WP`
only briefly while editing page tables and restores it afterward. The test also
asserts that changing the leaf never alters inherited parent RW/NX permissions.

With GNU-EFI, QEMU, OVMF, `sfdisk`, `mkfs.fat`, `mcopy`, and `timeout` installed,
run:

```sh
make -C kernel/uefi test-page-protection
```

The host-side runner copies the OVMF variable store into a temporary file and
maps QEMU exit status 33 to PASS (`0x10` written by the guest), 35 to FAIL
(`0x11`), and 37 to SKIP (`0x12`). SKIP means CPU NX, the firmware IDT, or the
four-level identity-mapped page-table layout is unsupported; it is not a
successful protection test. Other statuses and timeouts are harness errors.
Override `QEMU`, `OVMF_CODE`, or `OVMF_VARS` when the tools or firmware images
are installed elsewhere. A separate production image boot should also be
checked after loader or page-table changes; compilation and the isolated test
alone do not prove that the desktop survives the production handoff.

Run the production keyboard-focus regression under QEMU/OVMF with:

```sh
make -C kernel/uefi test-keyboard-focus
```

The test types while desktop focus is active and verifies that those characters
do not reach the shell, then returns focus to the terminal and executes `help`.
It saves a framebuffer capture at `kernel/uefi/out/keyboard-focus.ppm` for visual
inspection. QEMU and OVMF paths can be overridden with `QEMU`, `OVMF_CODE`, and
`OVMF_VARS` make variables.

Run the expanded terminal interaction regression with:

```sh
make -C kernel/uefi test-terminal-input
```

It additionally reconstructs commands using cursor movement, insertion,
Home/End/Right, Delete, and mid-line Backspace, then fills the bounded scrollback
and verifies that `PageUp`/`PageDown` change and restore the framebuffer view.
The captures are `kernel/uefi/out/terminal-scrollback.ppm` and
`kernel/uefi/out/terminal-tail.ppm`. The editor keeps input on the current
visible prompt row; the terminal follows new output by default, and the paging
keys move through the retained 64-line history.

Run the terminal window-controls regression with:

```sh
make -C kernel/uefi test-window-controls
```

The test drives the emulated PS/2 mouse under QEMU/OVMF and checks minimize,
maximize, restore, taskbar restore, close, and reopening the same terminal
session from Start > Terminal. The titlebar buttons are ordered minimize,
maximize/restore, and close from left to right. Close hides the terminal surface
without terminating the freestanding shell loop; the taskbar button or Start
menu can restore that in-memory session. The PS/2 setup now waits for device
ACK/RESEND responses so command replies cannot desynchronize subsequent mouse
packets. The final framebuffer is saved to
`kernel/uefi/out/window-controls.ppm`.

Run the terminal window-resize regression with:

```sh
make -C kernel/uefi test-window-resize
```

The QEMU/OVMF test checks inward/outward drags on all four edges, a bottom-right
corner resize, minimum window dimensions, and restoration of the original
bounds. Single-edge resizing keeps the opposite edge anchored; resizing is
clamped to the framebuffer work area, and the viewport is redrawn using the new
geometry. Existing scrollback lines are clipped rather than soft-wrapped or
reflowed. The framebuffer capture is
`kernel/uefi/out/window-resize.ppm`.

The UEFI desktop keeps Boot Services active so it can discover the EFI HTTP
protocol or its HTTP Service Binding protocol. When firmware provides it, the
terminal accepts:

```text
download http://host/path FILE
```

The response is bounded to 64 KiB and is written to the ESP. Firmware without
an HTTP driver is detected at boot and the command reports that the service is
unavailable. OVMF's default firmware generally lacks this protocol; this is a
firmware capability limitation, not a host-side download fallback.

## Termux

The supported Android workflow installs the verified BIOS artifact and a launcher:

```sh
pkg update
pkg install curl coreutils
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch-termux.sh | bash
fadalwatch tools
fadalwatch build check
fadalwatch run
```

`fadalwatch build` operates on `$FADALWATCH_SOURCE` when set, or `$HOME/Fadal-watch` by default. The guest remains an x86 BIOS image and is emulated by QEMU on Android.

## Network and RTL8139 testing

`fadalwatch run` attaches the same virtual network used by the kernel smoke test:

```sh
-netdev user,id=fadalnet -device rtl8139,netdev=fadalnet
```

The QEMU user network performs NAT on behalf of the guest. It does not bridge
the Android Wi-Fi adapter into FadalOS. The kernel should still detect the
RTL8139 through PCI, initialize RX/TX DMA, and report the Ethernet-II, ARP, and
IPv4 foundation checks. The current guest does not yet provide a complete DHCP,
DNS, TLS, or Git smart-HTTP client, so this is a driver and protocol test rather
than a promise of general Internet access.

To disable the emulated NIC for a comparison boot:

```sh
FADALWATCH_NETWORK=0 fadalwatch run
```

For a source-tree smoke test, use the equivalent canonical command:

```sh
make -C kernel QEMU=qemu-system-i386 check
```

For only the network path, use the Termux launcher diagnostic:

```sh
fadalwatch netcheck
```

It boots the verified image with an RTL8139 device, records the serial output
for eight seconds, and checks these three milestones: PCI/DMA initialization,
the ARP parser/cache self-test, and IPv4 receive/next-hop routing. The command
does not prove that the guest can browse the Internet; it verifies the native
driver and protocol foundation currently implemented by FadalOS.

## Artifacts

Build output belongs in `kernel/out` and `kernel/uefi/out`. Release-like copies belong in `artifacts`. Generated output should not be hand-edited or committed unless it is an explicitly versioned distribution artifact. Use the repository scripts to regenerate images.

## Failure diagnosis

If `make -C kernel check` fails, inspect `kernel/out/smoke.log` first. If the boot banner is absent, inspect the compiler, linker, boot sector size, and QEMU command. If only an individual trace is absent, locate the corresponding self-test in `kernel/kernel.c` and compare its ABI or initialization assumptions. For UEFI faults, first verify the payload header, loader allocation, GOP descriptor, and entry offset before changing renderer code.

## References

[1]: https://github.com/fadalstore/Fadal-watch "FadalWatch source repository"
[2]: https://www.qemu.org/docs/master/ "QEMU documentation"
