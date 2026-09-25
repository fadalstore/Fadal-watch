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

Available Phase 1 console commands are:

```text
help clear pwd ls cd cat whoami id uname fscan user drivers net restart reboot shutdown poweroff exit
```

The console accepts input from both UTM's **Terminal** serial window and the
emulated PS/2 keyboard. The payload handles Shift, Backspace, and Enter,
waits for UART transmit readiness, filters scan-code break events, and treats
CRLF from mobile terminals as one Enter key. This prevents repeated empty
prompts and garbled key sequences when using UTM SE.

The current console provides a root identity (`uid=0`), a registered non-root Fadal identity (`uid=1000`), virtual `/home/root` and read-only `/etc` views, and a loopback-only FScan security result. `cat /etc/dictionary` reports the immutable system-file policy. `restart`/`reboot` use the QEMU reset port, while `shutdown`/`poweroff` use the ACPI/ISA power-off ports supported by UTM/QEMU. **Persistent disk-backed `/home`, package installation, and UEFI-payload networking remain Phase 1B/Phase 2 work; the kernel image already has the RTL8139/DHCP path, but the UEFI desktop intentionally reports that its network service is not enabled yet. This image is not yet a Linux replacement.**

The bundle uses the current UTM configuration schema: `Backend=QEMU`, `ConfigurationVersion=4`, a `Drive` entry pointing to `Data/fadal-uefi.img`, UEFI enabled, an RTL8139 network adapter, and a built-in serial **Terminal**. This avoids the legacy `Drives`/`System` configuration format that older packages used and that UTM SE rejects as invalid. The refreshed image was boot-tested with OVMF/QEMU using GOP `1280x800`; this is the same UEFI framebuffer path used by UTM's VGA display.

UTM can import the bundle directly. If UTM SE still does not show an import action, unzip the archive first, open the resulting `FadalWatch-UEFI.utm` bundle in the Files app, and choose **Open in UTM SE**. As a fallback, create a new **Emulate → x86_64** VM, enable UEFI, and add `Data/fadal-uefi.img` as a raw IDE disk. The UTM serial mode should be **Terminal**.

A directly installable native `FadalOS.ipa` cannot be produced in the Linux sandbox because Apple requires an iOS signing certificate, provisioning profile, and device/app entitlements. The UTM bundle is the directly reusable emulator artifact.
