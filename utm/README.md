# FadalOS iOS/UTM Package

This package is a UTM QEMU-emulation bundle for **FadalOS Phase 1**, the x86_64 UEFI console system built from FadalWatch. It is intended for import into UTM and **UTM SE** on iOS or macOS. On ARM iPhone/iPad hardware, choose **Emulate**, not **Virtualize**; UTM SE does not provide hardware virtualization or JIT.

The 64 MiB raw image contains `EFI/BOOT/BOOTX64.EFI` and `FADAL.KRN`. After boot, the FadalOS console appears on the UTM serial terminal:

```text
FadalOS 0.1.0 / Fadal64 UEFI
fadal:/home/root#
```

Available Phase 1 console commands are:

```text
help clear pwd ls cd cat whoami id uname fscan user exit
```

The console accepts input from both UTM's **Terminal** serial window and the
emulated PS/2 keyboard. The payload handles Shift, Backspace, and Enter,
waits for UART transmit readiness, filters scan-code break events, and treats
CRLF from mobile terminals as one Enter key. This prevents repeated empty
prompts and garbled key sequences when using UTM SE.

The current console provides a root identity (`uid=0`), a registered non-root Fadal identity (`uid=1000`), virtual `/home/root` and `/etc` views, and a loopback-only FScan security result. **Persistent disk-backed `/home`, package installation, networking, and desktop services are Phase 1B/Phase 2 work; this image is not yet a Linux replacement.**

The bundle uses the current UTM configuration schema: `Backend=QEMU`, `ConfigurationVersion=4`, a `Drive` entry pointing to `Data/fadal-uefi.img`, UEFI enabled, an RTL8139 network adapter, and a built-in serial **Terminal**. This avoids the legacy `Drives`/`System` configuration format that older packages used and that UTM SE rejects as invalid.

UTM can import the bundle directly. If UTM SE still does not show an import action, unzip the archive first, open the resulting `FadalWatch-UEFI.utm` bundle in the Files app, and choose **Open in UTM SE**. As a fallback, create a new **Emulate → x86_64** VM, enable UEFI, and add `Data/fadal-uefi.img` as a raw IDE disk. The UTM serial mode should be **Terminal**.

A directly installable native `FadalOS.ipa` cannot be produced in the Linux sandbox because Apple requires an iOS signing certificate, provisioning profile, and device/app entitlements. The UTM bundle is the directly reusable emulator artifact.
