# FadalOS AI Handoff Guide

This file is the starting point for any future engineer or AI agent continuing FadalOS. Read it before editing code.

## Project identity

FadalOS is an independent freestanding operating-system project owned by `fadalstore/Fadal-watch`. The central requirement is independence from Linux and Ubuntu at runtime. Host Linux tools may compile and test the project, but no host kernel API, libc, or shell implementation may be copied into the guest.

## Current verified baseline

The latest verified Git commit is the repository `main` branch. The BIOS path builds and boots under QEMU. The UEFI path builds and boots through GOP at 1280x800. The graphical UEFI desktop contains a workspace, top bar, taskbar, PS/2 cursor, Start menu, unified keyboard/mouse event queue, a draggable/resizable terminal window with minimize/maximize/restore/close controls, and focus-aware keyboard routing.

The BIOS kernel currently includes preemptive scheduling foundations, dynamic processes, per-process address spaces, paging flags, page-fault handling, heap and slab allocation, FAT12 write/mount support, ATA PIO persistence, VFS, RAMFS, descriptor-backed file I/O, ring-3 `FSH.BIN`, a syscall ABI, TTY input, structured logging, a RAM disk, CPUID/SMP probes, synchronization primitives, an RTL8139 driver, Ethernet/ARP/IPv4 foundations, UDP/TCP prototypes, and Git protocol groundwork.

## First commands

```sh
git clone https://github.com/fadalstore/Fadal-watch.git
cd Fadal-watch
scripts/validate-repo.sh
```

On Termux, use the dedicated installer and launcher:

```sh
pkg update
pkg install curl coreutils
curl -fsSL https://raw.githubusercontent.com/fadalstore/Fadal-watch/main/scripts/install-fadalwatch-termux.sh | bash
fadalwatch tools
fadalwatch build check
```

## Source map

`kernel/kernel.c` is the BIOS kernel integration point. It contains initialization, the syscall dispatcher, process lifecycle, scheduler hooks, TTY behavior, and the boot trace. `kernel/fsh.c` is the standalone ring-3 shell. `kernel/vfs.c`, `kernel/fat12.c`, `kernel/ramfs.c`, and `kernel/ata.c` implement storage layers. `kernel/fnet.c`, `kernel/rtl8139.c`, and `kernel/gitpack.c` implement the current network path. `kernel/uefi/boot.c` is the UEFI loader. `kernel/uefi/payload.c` is the 64-bit graphical payload. `kernel/uefi/payload.ld` defines the flat payload layout. `tools/patch-fadal64-header.py` patches the FDL64 image header.

## Required workflow

Before a change, read the relevant source and its header. Make the smallest coherent change. Build the affected target. Run `scripts/validate-repo.sh` or the narrower target. Inspect `git diff --check`. Commit a focused change. Push it to GitHub after verification so the next session has a recoverable checkpoint.

Never overwrite a stable artifact with an untested one. Never claim a UEFI graphical feature is complete from compilation alone; boot it under QEMU or UTM and inspect the framebuffer. Never modify the syscall numbers already used by `FSH.BIN`; append new ABI entries instead.

## Compatibility contracts

The BIOS smoke test in `kernel/Makefile` is a contract. Its required serial lines must remain present unless the test is intentionally updated with a documented behavior change. The FDL64 header is 32 bytes and contains the magic, version, header size, image size, and entry offset. The UEFI loader validates those fields before handoff. The UTM configuration uses emulation for x86_64 and must remain compatible with UTM SE configuration version 4.

## Current next milestones

The UEFI payload now has an FDL64 v3 memory contract: the fixed 32-byte base
header is preserved, and a 40-byte `FPM1` descriptor immediately after it
records page-aligned text, rodata, data, and BSS extents. The loader allocates
page-aligned pages, copies the file, zeroes the BSS tail, validates the
descriptor, enables NXE and `CR0.WP`, and installs `R/NX`, `RX`, `R/NX`, and
`RW/NX` permissions before entering the payload. `kernel/uefi/page_tables.c`
is shared by the production loader and the EFI self-test; it walks four-level
identity mappings, splits 1 GiB/2 MiB entries as needed, preserves inherited
parent restrictions, and edits only payload leaf PTEs. FDL64 v2 remains
loadable for compatibility but runs without W^X and produces a warning.

The input-routing milestone now gives the terminal, desktop, and Start menu
explicit focus states. `Tab` toggles terminal/desktop focus, Escape returns
from non-terminal focus, mouse clicks select the terminal or Start menu, and
PS/2 AUX bytes are separated from keyboard bytes. Shell keystrokes are ignored
unless the terminal owns focus; UART input remains an explicit path back to the
terminal.

The BIOS PS/2 TTY now tracks left/right Shift independently, supports Caps Lock
with standard Shift XOR Caps letter behavior, and maps US shifted digits and
punctuation. `keyboard_translation_self_test` covers the modifier combinations
and representative symbols; the BIOS boot smoke test requires its pass trace.

The UTM pointer-jitter fix makes desktop clicks edge-triggered: a held left
button or touchscreen contact acts once, while drag/resize continues until
release. Unchanged PS/2 mouse packets are discarded, and ordinary pointer motion
updates the saved-under cursor instead of repainting the entire GOP desktop.
`test-window-controls` checks that moving a held pointer over Start does not
repeatedly toggle the menu.

**P2.9 complete (source/runtime checkpoint):** the BIOS FSH ring-3 probe attempts
to write its own read-only code byte. Recovery is permitted only for the exact
PID 1 / CS `0x1b` / error `0x7` / CR2 / store-EIP match, with the process mapping
verified read-only and the target byte unchanged; unrelated faults remain
fail-closed. The near-match self-test and post-recovery FSH banner are required
by `make -C kernel check`. The final validated sizes were 48,953/61,440 bytes for
the kernel and 3,873/4,096 bytes for FSH.BIN. No new UTM ZIP was generated and
existing release archives were not changed. See [P2.9 progress](docs/HORUMAR_P2_9.md)
and [the Somali product roadmap](docs/ROADMAP_FADALWATCH_SO.md).

**P2.10 complete for its scoped milestone (hostname remains unverified):**
`make -C kernel/uefi test-network-audit` passed under QEMU/OVMF with `e1000` and
`restrict=on`. Three SNP handles were present; `LocateProtocol(EFI_HTTP)` returned
`Not Found`, while HTTP Service Binding, `CreateChild`, and `OpenProtocol`
succeeded, providing an EFI_HTTP child. DHCP acquired station `10.0.2.15/24`.
A bounded numeric-IP plaintext GET to the local `guestfwd` fixture returned and
saved `FADAL_UEFI_HTTP_FIXTURE_OK`. The initial hostname attempt failed when
`EFI_HTTP.Configure` returned `Already started` at stage 4; it did not establish
anything about DNS. General Internet and HTTPS/TLS were not tested. Earlier UTM
archives were not changed. The [P2.10 UTM candidate](FadalOS-Phase2.10-UTM-iOS.zip)
is 161,481 bytes with SHA-256
`87ec98d8163da8ae0653fb7f615473452ad4b152a1617d21174bb94482b10cc9`. Its
packaging checks passed; the extracted UEFI disk booted under OVMF and passed the
DHCP/local-fixture regression, while the BIOS disk matched the QEMU-tested image
byte-for-byte. Earlier Phase 1/P2.5/P2.6/P2.8 archives remain unchanged. Package
and release record are pushed in commit `6f10c93`. See
[P2.10 progress](docs/HORUMAR_P2_10.md).

**P2.11 current status:** configure-once was added for the EFI_HTTP child and the
regression now verifies two sequential numeric-IP GETs; both reached the local
fixture and returned `FADAL_UEFI_HTTP_FIXTURE_OK`. OVMF reports DNS4 Service
Binding available but `EFI_DNS4` protocol instance unavailable. The repeated
`10.0.2.100.nip.io` hostname probe now fails at `EFI_HTTP.Request` with
`stage=5 status=Not Found HTTP=0`; the fixture saw no new hostname request. The
30-frame QEMU pcap contains six DHCP UDP frames and no UDP/TCP port 53 or hostname
TCP session. This does not distinguish an internal DNS failure from an HTTP
request/transport failure. The System framebuffer was inspected at
`kernel/uefi/out/network-audit-p2.11-system.png`; the pcap is at
`kernel/uefi/out/network-audit-p2.11.pcap`. Next, create a DNS4 child through the
available Service Binding, configure it, and test `HostNameToIp` separately
before deciding whether a native DNS resolver is required. Hostname/DNS, general
Internet, and HTTPS/TLS remain unverified. Do not overwrite P2.10 or any earlier
versioned UTM archive. See [P2.11 progress](docs/HORUMAR_P2_11.md) for commands,
limits, and source references; the P2.10 release hash remains
`87ec98d8163da8ae0653fb7f615473452ad4b152a1617d21174bb94482b10cc9`.

**P2.12 current status:** the UEFI loader now creates a DNS4 child through the available
Service Binding when `LocateProtocol(EFI_DNS4)` has no instance, opens the child,
configures it with DHCP/default DNS settings, and submits a bounded `HostNameToIp`
probe. QEMU/OVMF verified child creation, Configure, and query submission. The
hostname HTTP probe still failed at `EFI_HTTP.Request` with `stage=5 status=Not Found
HTTP=0`; the fixture saw no request and the P2.12 pcap contained zero port-53
packets. DNS completion/transport, general Internet, and HTTPS/TLS remain unverified.
The network audit now protects the final direct-or-child DNS4 instance state and
captures `kernel/uefi/out/network-audit-p2.12.pcap`; see [P2.12 progress](docs/HORUMAR_P2_12.md).

**Files** ee Start hadda waxa uu furaa browser framebuffer ah oo adeegsada
home-list iyo read callbacks-ka ESP. Wuxuu taxaa `/home/root`, wuxuu geli karaa
folder-hoosaadyo kana soo noqon karaa parent-ka, wuxuuna soo bandhigaa labada sadar
ee ugu horreeya faylka ASCII-ga la doortay; preview-gu waa akhris-keliya.
Files-ku hadda waxa kale oo uu taageeraa keyboard navigation: Up/Down waxay
dhaqaajiyaan highlight-ka safafka muuqda, Enter wuxuu furaa folder ama preview-gareeyaa
fayl, Home/End waxay u boodaan bilowga/dhammaadka, Backspace/Left-na waxay ku noqdaan
parent-ka. Liisku wuxuu leeyahay 15-row viewport oo ku rogmada bounded 4 KiB
directory buffer; marka service-ku dhaafo xadka waxa la muujiyaa fariin cad.
`test-window-controls` wuxuu ku daraa 20 fixture files oo ku jira nuqul image ku-meelgaar
ah, wuxuuna xaqiijiyaa in End/Up gaadhaan tail-ka, Enter furo item-ka, Home-na soo celiyo
bogga koowaad. Waxa kale oo uu tijaabiyaa Backspace Downloads ilaa `/home/root`.
**System** waxa uu furaa panel muujinaya x86_64, UEFI/GOP, ilaalinta xusuusta FDL64
v3, helitaanka persistent home, iyo UEFI HTTP. Escape ama badhanka close ayaa
xira panel kasta; taskbar-ku wuxuu dib u soo celiyaa terminal-ka. Start-ka waxa
lagu daray **Fadal Browser**: URL waxaa lagu geliyaa keyboard-ka, firmware UEFI
HTTP ayuu GET ku sameeyaa, HTML tags-ka caadiga ahna wuxuu uga soo saaraa ASCII
text preview (xadka akhrisku waa 4 KiB). HTTPS/TLS, JavaScript, CSS, sawirro,
redirects iyo clickable links ma taageero. Bogga iyo downloads-ka ilaa 64 KiB
waxaa lagu kaydiyaa `/home/root/Downloads`; badhanka Downloads-ku wuxuu folder-kaas
ka furaa Files. Boot-ka `test-window-controls` ma laha NIC wuxuuna hubiyaa fariinta
adeegga maqan; `test-network-audit` waa tijaabada goonida ah ee e1000/HTTP fixture.
`test-window-controls` hadda wuxuu xaqiijiyaa Files keyboard selection, preview,
Backspace parent navigation, System panel, Browser launch, keyboard URL edit/Enter,
HTML-to-text self-test, iyo Downloads shortcut-ka; ma sheeganayo in fetch Internet
toos ah lagu tijaabiyay QEMU.

The UEFI terminal interaction milestone is implemented: Left/Right, Home/End,
insertion at the cursor, Backspace, and Delete edit the current command; PageUp
and PageDown page through the bounded 64-line scrollback. The editor accepts up
to 95 characters in fixed storage and horizontally scrolls the visible prompt
slice to keep the cursor in view, independent of window width. The terminal
returns to the live tail when a new prompt begins.

The established regression commands are `scripts/validate-repo.sh --uefi`,
`make -C kernel/uefi test-page-protection`,
`make -C kernel/uefi test-keyboard-focus`,
`make -C kernel/uefi test-terminal-input`,
`make -C kernel/uefi test-terminal-selection`,
`make -C kernel/uefi test-terminal-history`,
`make -C kernel/uefi test-window-controls`,
`make -C kernel/uefi test-window-resize`, and
`make -C kernel/uefi test-persistent-home`, and
`make -C kernel/uefi test-network-audit`. The focus regression boots the
production ESP under QEMU/OVMF, proves desktop-focused typing does not reach the
shell, then returns focus and executes `help`. The terminal-input regression
reconstructs commands through cursor editing and verifies that PageUp/PageDown
change and restore the framebuffer view. Inspect
`kernel/uefi/out/keyboard-focus.ppm`,
`kernel/uefi/out/terminal-scrollback.ppm`,
`kernel/uefi/out/terminal-tail.ppm`,
`kernel/uefi/out/terminal-selection.ppm`,
`kernel/uefi/out/terminal-history.ppm`, and
`kernel/uefi/out/window-controls.ppm`. The window regression drives the PS/2
mouse and verifies maximize/restore geometry, minimize/taskbar restore, close,
and reopening the retained terminal session from Start. The isolated protection test must report
both the NX instruction-fetch fault (`error=0x11`) and read-only write fault
(`error=0x3`); the linker should show separate `R`, `R E`, `R`, `RW` LOAD
segments with no RWX warning. Terminal resizing is implemented on all four
edges and corners. Opposite edges stay anchored for single-edge drags; the
window enforces minimum dimensions and framebuffer work-area bounds. Scrollback
records whether each boundary is an explicit newline or a soft wrap, then
reflows retained text to the new width while preserving the user's visible
scrollback anchor. The active command remains bounded to 95 characters
independent of window width; the visible prompt slice scrolls horizontally
around the cursor instead of discarding hidden input. The QEMU/OVMF regression
types a 45-character command, shrinks to minimum width, restores the original
width, and compares the terminal framebuffer region before submitting it. It also verifies
repeated shrink/expand cycles, restoration of long output, command editing during
resize, PageUp/PageDown behavior across reflow, edge/corner drags, minimum
dimensions, and restored original bounds. Its final geometry capture is
`kernel/uefi/out/window-resize.ppm`. Keep the BIOS
shell and UEFI desktop as separate deliverables. Always boot-test loader or
compositor changes under QEMU/UTM and inspect the framebuffer.

Terminal text selection and guest-local copy/paste are implemented. Left-button
drag selects visible row/column cells, `Ctrl+C` copies into a fixed 4 KiB buffer,
and `Ctrl+V` inserts printable text at the active command cursor, converting
line breaks to spaces and respecting the 95-character command bound. Selection
highlight is cancelled on focus loss, output, scrollback movement, and resize;
the guest clipboard remains independent of any host clipboard. The standalone
QEMU/OVMF regression verifies the highlight, copy/paste of `help`, focus/resize
cancellation, and retains `kernel/uefi/out/terminal-selection.ppm`. Both the
standalone selection regression and the broader terminal-input regression passed
in the current validation run.

The UEFI terminal command history is implemented as an eight-entry fixed ring of
non-empty submitted commands. Up walks newest to oldest, Down walks forward and
restores the draft captured before the first recall, and recalled commands remain
editable. Empty submissions are not added; history is volatile guest memory and
does not survive reboot. `make -C kernel/uefi test-terminal-history` passed under
QEMU/OVMF and checks empty submission, recall, draft restore, recalled-command
editing, and wraparound after nine distinct commands. The `history` builtin
lists the retained eight entries oldest-to-newest, including its own invocation
as the newest normal submission. The regression compares the exact ordered list.
Inspect `kernel/uefi/out/terminal-history.ppm`.

The Phase 2 release gate passed `scripts/validate-repo.sh --uefi` and the full
UEFI QEMU/OVMF matrix: page protection, focus, terminal editing/scrollback,
selection/guest clipboard, eight-entry history, window controls and Files
navigation, edge/corner resizing and reflow, and persistent-home guest reboot
plus raw-ESP host checks. The terminal-input regression now checks for the
expected result in fresh serial output after each editing sequence and allows
at most three attempts; no command-editing assertions were removed. The
window-controls key helper remains bounded and waits for visible titlebar and
selection state; the final suite reported one recovered QEMU HMP `Down` event.
The persistent-home guest and host checks use the exact same image.

Phase 2 is tracked in `docs/PHASE2_PLAN.md`. P2.1 (the bounded UEFI `history`
builtin), P2.2 (test-suite reliability), P2.3 (BIOS runtime architecture
decision), P2.4 (bounded BIOS memory-window proof), P2.5 (release checkpoint),
P2.6 (bounded BIOS loader capacity), and P2.7 (BIOS supervisor write
protection) are complete. [ADR 0001](docs/adr/0001-bios-runtime-architecture.md)
keeps the existing i386 BIOS runtime and defers a separate x86-64 BIOS path.
The BIOS identity map is 64 MiB, backed only by E820 type-1 pages; when E820 is
unavailable, allocator access stays capped at 16 MiB. User mappings remain
private while identical supervisor-only tables above the first 8 MiB are
shared. The current BIOS loader reserves up to 120 sectors (61,440 bytes),
stores the actual rounded-up sector count in its 512-byte boot sector, and
rejects zero or more than 120 before INT 13h. For images built by the current
Makefile, the kernel occupies LBAs 1–120 and FAT12 starts at LBA 121 (data LBA
125); `FAT12_DISK_LBA` is derived from the same Make variable. Phase 1 packages
and the P2.5 Phase 2 ZIP retain their original 112-sector/LBA-113 images and
remain unchanged. `make -C kernel check` passes the original ABI/smoke suite,
exact 61,440-byte guard acceptance and one-byte-over rejection, QEMU boot at
the full 120-sector load count, QEMU rejection of count 121 before kernel
entry, and the 16/32/64 MiB high-page allocator tests. Syscall 1–34 and legacy
smoke traces remain unchanged.

The frozen P2.5 [Phase 2 UTM package](FadalOS-Phase2-UTM-iOS.zip) has SHA-256
`7856305f788f26ce93a4d407696615c9b51919224eb5ed7b15b2c8bbdc1365f5`; the UEFI
The newer [P2.6 UTM package](FadalOS-Phase2.6-UTM-iOS.zip) is a distinct archive
with SHA-256 `220d3f185298878a4fc05a076a383dc18f31020ef0f4f098e2b6b429660e3825`.
Its BIOS disk SHA-256 is `37e10c0376881d389df6364c435271d0adbcdfb01ada3160656d3f6e210aa77f`,
and its UEFI disk SHA-256 is `537cee4bb9dbb7ee16993d92e83873b75a2c77245b57833ace047c3c8ffd86da`.
Both images extracted from this ZIP booted under QEMU at 128 MiB `pc` and under
OVMF with focus/keyboard regression respectively. Its archive and internal image
checksums passed. The Phase 1 UTM ZIP aliases remain unchanged at SHA-256
`bd1b433309cce3671f9a206f56629db8d7db8268a0167a378696db705c362fab`. The
current BIOS payload is 57,273/61,440 bytes, leaving 4,167 bytes in the new
120-sector window; this figure does not describe the frozen P2.5 ZIP.

P2.7 adds page-aligned BIOS `.text`/`.rodata` read-only mappings and a separate
writable `.data`/BSS range. The kernel enables `CR0.WP` only after paging, GDT,
and IDT setup; its page-fault handler recovers only the exact controlled
supervisor store to a harmless read-only byte. Both process address spaces are
checked for preserved kernel permissions, and syscall copyout rejects the
read-only user-code page. FSH's mutable buffers and authentication state now
reside on its writable user stack. This is not NX support: BIOS remains
32-bit non-PAE. `make -C kernel check` passes the legacy BIOS contract, memory
matrix, 120-sector loader guards, expected CR0.WP fault, and process mapping
checks; `scripts/validate-repo.sh --uefi` also passes. Every UEFI QEMU/OVMF
target passed across the serial suite and targeted reruns. The first aggregate
run had transient HMP/focus timing failures in terminal-history and the
persistent-home startup; both targets passed when run alone. At the P2.7
checkpoint, the BIOS payload was 61,305/61,440 bytes, leaving 135 bytes. The
P2.6 ZIP and Phase 1 archives remain frozen and do not include P2.7.

**P2.8 complete (source + separately versioned UTM package):** the BIOS default
compiler optimization is now `-Os` instead of `-O2`. The binary is
47,609/61,440 bytes, saving 13,696 bytes and leaving 13,831 bytes of loader
headroom; the boot sector records 93 sectors while the existing 120-sector
ceiling and LBA-121 FAT12 layout remain unchanged. `make -C kernel check` and
`scripts/validate-repo.sh --uefi` passed. The new
`FadalOS-Phase2.8-UTM-iOS.zip` is 159,288 bytes with SHA-256
`b2f421d86f641a9bbec185cc6a5189cbff4ce8d7b5246bec3eb6af0bd2ac151a`; its UEFI
and BIOS disk SHA-256 values are `14d518b61d7b8deb4af45eb9244d552c565a799996cd5f94dc65207eb5dbe4a2`
and `e7e559013566b64090e1e3580adb4ac2463ef8ddd949311557606cbe9da68b88`.
Archive checksums, internal image hashes, BIOS QEMU boot, UEFI OVMF
window-controls/focus, and EFI page-protection tests passed. P2.6 and Phase 1
archives remain unchanged. Source commit `f97422e` and the P2.8 package follow-up
are pushed to GitHub `main`. See the Somali [P2.8 progress note](docs/HORUMAR_P2_8.md)
and [release record](docs/PHASE2_RELEASE.md).

**Checkpoint note:** P2.9 is complete as described above. Keep versioned UTM
archives unchanged unless a separate release gate is approved. Persistent command
history remains out of scope until storage permissions and privacy semantics are
explicitly designed.

The persistent-home view is backed by the ESP's `FADALHOME` directory through
validated loader callbacks. `ls`, `cat PATH`, `write PATH TEXT`, `mkdir PATH`,
and `rm PATH` use EFI file operations; first boot seeds README, motd, and profile
files only when missing. Files can live in nested directories created one level
at a time (each parent must already exist); writes replace old content, are
capped at 4096 bytes, and survive a guest reboot on the same disk. `rm` removes
files only, and EFI metadata checks prevent directory deletion or replacement.
The QEMU/OVMF test verifies listing, reading, overwrite truncation, directory
creation, nested file operations, path confinement, reboot persistence, safe
deletion, and exact file bytes extracted from the FAT image. This is not yet a
general VFS: per-user isolation, directory removal, and power-loss journaling
remain unimplemented.

The shell path-resolution slice is implemented with a bounded current-working-
directory resolver initialized at `/home/root`; the live path appears in the
prompt. `pwd`, `cd [PATH]`, `ls [PATH]`, `cat PATH`, `write PATH TEXT`,
`mkdir PATH`, and `rm PATH` accept normalized absolute or relative paths with
`.` and `..`. `/etc` files are read-only; writes and deletes remain confined to
`/home/root`, and `mkdir` requires its parent directory to exist.
The persistent-home regression also verifies `cd /etc`, `pwd`, a relative read of
`motd`, relative `cd` back to `/home/root`, and reading the seeded README there.
It creates a directory and nested file, confirms both survive reboot, then
removes the nested file and verifies the directory is neither removable nor
replaceable by `write`.

**TCP receive slice:** `kernel/fnet.c` hadda wuxuu leeyahay 4 KiB ring buffer oo bounded ah, checksum-ku wuxuu daboolaa TCP segments waaweyn oo ay ku jiraan payload dhererkiisu odd yahay, waxaana la aqbalaa oo keliya xogta in-order ah ee boos ku filan haysa. TCP window-ga la xayeysiiyo wuxuu la socdaa booska bannaan; duplicate, gap, ama segment ka weyn booska jira lama tirtiro si khaldan—ACK-gu wuxuu hayaa receive sequence-kii, si sender-ku dib ugu soo gudbiyo. Handshake-ku hadda wuxuu cunaa hal sequence number-ka SYN, SYN retry-guna wuxuu isticmaalaa ISN-kii hore; SYN-ACK-ga waxaa laga rabaa ACK=ISN+1. `make -C kernel check` wuxuu ku tijaabiyaa handshake-kan, frame truncated ah, multi-segment stream, partial reads, duplicate/gap, backpressure, iyo ring wrap.

**Git pkt-line reader:** `kernel/fnet.c` hadda wuxuu hayaa parser incremental ah oo qaata header 4-byte ah iyo payload marka TCP reads-ku u kala go'aan. Wuxuu aqbalaa `0000` flush, `0001` delimiter, iyo `0002` response-end; wuxuu soo saaraa SHA-1-ka ref-ka koowaad marka pkt-line-kiisu dhammaystirmo, wuxuuna diidaa header aan hex ahayn ama dherer xogeed ka yar 4. `fnet_git_clone_poll` wuxuu adeegsanayaa reader-kan. Self-test-ku wuxuu u kala jarayaa header/payload dhowr akhris wuxuuna hubiyaa dherer khaldan. Tani wali ma hirgelin codsiga packfile-ka, decoding/persisting objects, SHA-256 object IDs, ama TLS; QEMU boot test ayaa loo baahan yahay si loo xaqiijiyo self-test-ka dhexdiisa kernel-ka.

**HTTP parser + TCP adapter test:** `kernel/http.c`/`kernel/http.h` waa parser response HTTP/1 ah oo incremental ah, xaddidanna leh; wuxuu taageeraa `Content-Length`, `Transfer-Encoding: chunked` (oo trailers leh), 1xx interim response, iyo body ku dhammaada EOF. `kernel/fnet.c` hadda wuxuu sidoo kale ku tijaabiyaa response synthetic ah oo TCP segments kala go'an lagu geliyo receive ring-ka, kaddibna `fnet_tcp_read` qaybihiisa si toos ah loogu quudiyo parser-ka; test-ku wuxuu xaqiijiyaa status 200 iyo body-ga `hello world` marka headers/body-gu kala go'aan xuduudaha TCP/read-ka. Tani waa seam regression oo keliya—wali ma jiro HTTP request client live ah, TLS, ama Git smart-HTTP. Parser-ku wuxuu diidaa framing is-khilaafsan/laba-jeer ah, coding aan la taageerin, status khaldan, iyo EOF aan dhammaystirnayn. `make -C kernel check` wuxuu daboolaa fixed/chunked/close-delimited bodies, `100 Continue`, malformed framing, iyo TCP adapter-ka.


## What not to assume

The UEFI payload and BIOS kernel do not share C globals or syscall implementations. The UEFI desktop is not yet a complete userspace desktop environment. The native network stack is not yet a production HTTPS client. The GitHub bridge is a safe command/status bridge, not a claim that the guest can clone over HTTPS. The current scheduler and SMP support are staged implementations with explicit single-BSP guards.

## Definition of done for a milestone

A milestone is complete only when the source is readable, the relevant build passes, the runtime behavior is boot-tested, the documentation is updated, the repository is clean, and the commit is pushed. Include the exact test command and artifact path in the handoff message.

## References

[1]: https://github.com/fadalstore/Fadal-watch "FadalWatch source repository"
[2]: https://github.com/termux/termux-packages "Termux package definitions and build environment"
