# FadalOS iOS/UTM Package

This package contains two separate UTM QEMU-emulation profiles for **FadalWatch**: the x86_64 UEFI graphical environment and the 32-bit BIOS kernel. Both are intended for import into UTM and **UTM SE** on iOS or macOS. On ARM iPhone/iPad hardware, choose **Emulate**, not **Virtualize**; UTM SE does not provide hardware virtualization or JIT.

The default desktop profile is `FadalWatch-UEFI.utm`. Its 64 MiB raw image contains `EFI/BOOT/BOOTX64.EFI` and `FADAL.KRN`. After boot, the FadalOS console appears on the UTM serial terminal:

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

The desktop Start menu includes **Fadal Browser**, a small native HTTP text viewer with a URL field, **Go**, **Download**, and **Downloads** controls. It uses the firmware UEFI HTTP service for plaintext `http://` GET requests, strips basic HTML markup into text, and persists cached pages and downloads under `/home/root/Downloads`, which opens in Files. Browser downloads are capped at 64 KiB and page preview reads up to 4 KiB. HTTPS/TLS, JavaScript, CSS, images, clickable links, and redirects are not implemented. The command `download http://host/path FILE` also saves below `/home/root`. Firmware must publish UEFI HTTP; the default OVMF/QEMU test environment does not, and the browser reports that clearly rather than implying Internet access. The browser's QEMU tests therefore validate app launch, URL keyboard entry, HTML text conversion, status handling, and the Downloads shortcut, not live network fetching.

The terminal has a persistent `/home/root` tree backed by `FADALHOME` on the FAT32 EFI System Partition. `pwd`, `cd [PATH]`, `ls [PATH]`, `cat PATH`, `write PATH TEXT`, `mkdir PATH`, and `rm PATH` are available; paths may be absolute or relative and are confined to home storage for writes/deletes. `mkdir` creates one level at a time; `rm` deletes files but refuses directory targets. Shell-written files are capped at 4096 bytes; browser/HTTP downloads are capped separately at 64 KiB. Seeded README, motd, and profile files are created only when absent. Storage survives a guest restart when UTM retains the same disk image. This remains an initial home-filesystem layer rather than a full VFS: per-user isolation, directory removal, and power-loss journaling are not implemented. The terminal accepts the built-in PS/2 keyboard and UTM's serial Terminal; `restart`/`reboot` and `shutdown`/`poweroff` use the QEMU/ACPI controls.

The UEFI bundle uses the current UTM configuration schema: `Backend=QEMU`, `ConfigurationVersion=4`, a `Drive` entry pointing to `Data/fadal-uefi.img`, UEFI enabled, an RTL8139 network adapter, and a built-in serial **Terminal**. This avoids the legacy `Drives`/`System` configuration format that older packages used and that UTM SE rejects as invalid. The refreshed image was boot-tested with OVMF/QEMU using GOP `1280x800`; this is the same UEFI framebuffer path used by UTM's VGA display.

## BIOS kernel profile

`FadalWatch-BIOS.utm` is a separate x86_64 QEMU machine using legacy BIOS (`UEFIBoot=false`, `System.Target=pc`) and `Data/fadal-kernel.img`. That raw disk starts the independent 32-bit BIOS kernel; the latest image includes the TCP receive work and bounded HTTP/1 response parser. It was boot-tested with QEMU SeaBIOS in `pc` mode, including the TCP and HTTP parser serial self-tests. Select this profile to test the BIOS kernel; it does not replace or modify the UEFI desktop disk. The BIOS parser handles plaintext HTTP framing only and does not provide TLS/HTTPS.

The ZIP contains both `.utm` profiles. Import `FadalWatch-UEFI.utm` for the Fadal64 graphical desktop or `FadalWatch-BIOS.utm` for the 32-bit BIOS kernel. If UTM SE does not show an import action, unzip first, open the selected bundle in the Files app, and choose **Open in UTM SE**. As a fallback, create an **Emulate → x86_64** VM: enable UEFI and add `Data/fadal-uefi.img` for the desktop, or disable UEFI and add `Data/fadal-kernel.img` for BIOS. The serial mode should be **Terminal**.

A directly installable native `FadalOS.ipa` cannot be produced in the Linux sandbox because Apple requires an iOS signing certificate, provisioning profile, and device/app entitlements. The UTM bundle is the directly reusable emulator artifact.
