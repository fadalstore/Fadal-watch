# FadalOS iOS/UTM Package

This package is a UTM QEMU-emulation bundle for **FadalOS Phase 1**, the x86_64 UEFI console system built from FadalWatch. It is intended for import into UTM and **UTM SE** on iOS or macOS. On ARM iPhone/iPad hardware, choose **Emulate**, not **Virtualize**; UTM SE does not provide hardware virtualization or JIT.

The 64 MiB raw image contains `EFI/BOOT/BOOTX64.EFI` and `FADAL.KRN`. After boot, the FadalOS console appears on the UTM serial terminal:

```text
FadalOS 0.1.0 / Fadal64 UEFI
fadal:/home/root#
```

With the UTM display enabled, the same boot now renders the first FadalOS
desktop: a blue workspace, `FADAL OS` top bar, desktop welcome card, terminal
panel, status indicator, and `START` taskbar. The framebuffer is supplied by
UEFI GOP and drawn directly by the independent Fadal64 payload.

The Start menu's **Files** entry opens a basic read-only browser for the
persistent `/home/root` tree. It displays ESP-backed entries, lets you open
nested folders and go up to the parent, and previews the first two lines of a
selected file. Use the terminal's `write` and `rm` commands to change files.
**System** opens a status panel showing the x86_64/UEFI/GOP runtime, FDL64 v3
memory-protection mode, and whether the firmware exposes persistent storage and
HTTP. Close either panel with Escape or its titlebar close button; the taskbar
restores the terminal.

Available Phase 1 console commands are:

```text
help clear pwd ls cd cat write whoami id uname fscan user drivers net download restart reboot shutdown poweroff exit
```

The console accepts input from both UTM's **Terminal** serial window and the
emulated PS/2 keyboard. The payload handles Shift, Backspace, and Enter,
waits for UART transmit readiness, filters scan-code break events, and treats
CRLF from mobile terminals as one Enter key. This prevents repeated empty
prompts and garbled key sequences when using UTM SE.

UTM's emulated PS/2 pointer treats a held touch as one click instead of
repeating menu or window actions. Moving the pointer redraws only its small
cursor area, not the entire desktop; drag the terminal by its title bar and
resize it from an edge or corner.

The current console provides a root identity (`uid=0`), a registered non-root Fadal identity (`uid=1000`), a persistent flat `/home/root` directory, read-only `/etc` views, and a loopback-only FScan security result. The prompt shows the current directory; use `pwd` to print it and `cd [PATH]` to change it. `ls [PATH]`, `cat PATH`, `write PATH TEXT`, `mkdir PATH`, and `rm PATH` accept absolute or relative paths with `.` and `..`. Home files live under `FADALHOME` on the FAT32 EFI System Partition. `mkdir PATH` creates one directory at a time under `/home/root` (its parent must exist). `write PATH TEXT` replaces a file with one line of text, and `rm PATH` deletes files at any depth while refusing directory targets. Writes and deletes stay within `/home/root`; path components are limited to 47 ASCII characters and files to 4096 bytes. The seeded README, motd, and profile are created only when absent. Files survive a guest restart as long as UTM retains the same disk image. This is an initial persistent-home layer, not yet a general VFS: per-user storage isolation, directory removal, and power-loss journaling are not implemented. `cat /etc/dictionary` reports the immutable system-file policy. `restart`/`reboot` use the QEMU reset port, while `shutdown`/`poweroff` use the ACPI/ISA power-off ports supported by UTM/QEMU. When firmware publishes the UEFI HTTP protocol or its Service Binding protocol, `download http://host/path FILE` performs a bounded HTTP GET and writes the response to the ESP. The desktop detects this capability at boot and reports `HTTP service unavailable` instead of pretending to have Internet access when the firmware does not publish it. OVMF's default build used for local QEMU testing does not publish the HTTP service; UTM/UTM SE must provide the protocol for this command to work. HTTPS/TLS remains future work.

The bundle uses the current UTM configuration schema: `Backend=QEMU`, `ConfigurationVersion=4`, a `Drive` entry pointing to `Data/fadal-uefi.img`, UEFI enabled, an RTL8139 network adapter, and a built-in serial **Terminal**. This avoids the legacy `Drives`/`System` configuration format that older packages used and that UTM SE rejects as invalid. The refreshed image was boot-tested with OVMF/QEMU using GOP `1280x800`; this is the same UEFI framebuffer path used by UTM's VGA display.

UTM can import the bundle directly. If UTM SE still does not show an import action, unzip the archive first, open the resulting `FadalWatch-UEFI.utm` bundle in the Files app, and choose **Open in UTM SE**. As a fallback, create a new **Emulate → x86_64** VM, enable UEFI, and add `Data/fadal-uefi.img` as a raw IDE disk. The UTM serial mode should be **Terminal**.

A directly installable native `FadalOS.ipa` cannot be produced in the Linux sandbox because Apple requires an iOS signing certificate, provisioning profile, and device/app entitlements. The UTM bundle is the directly reusable emulator artifact.
