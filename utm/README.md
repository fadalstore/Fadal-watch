# FadalOS iOS/UTM Package

This package is a UTM QEMU-emulation bundle for **FadalOS Phase 1**, the x86_64 UEFI console system built from FadalWatch. It is intended for import into UTM on iOS or macOS. On ARM iPhone/iPad hardware, choose **Emulate**, not **Virtualize**.

The 64 MiB raw image contains `EFI/BOOT/BOOTX64.EFI` and `FADAL.KRN`. After boot, the FadalOS console appears on the UTM serial terminal:

```text
FadalOS 0.1.0 / Fadal64 UEFI
fadal:/home/root#
```

Available Phase 1 console commands are:

```text
help clear pwd ls cd cat whoami id uname fscan user exit
```

The current console provides a root identity (`uid=0`), a registered non-root Fadal identity (`uid=1000`), virtual `/home/root` and `/etc` views, and a loopback-only FScan security result. **Persistent disk-backed `/home`, package installation, networking, and desktop services are Phase 1B/Phase 2 work; this image is not yet a Linux replacement.**

UTM can import the bundle directly or import `Data/fadal-uefi.img` as a drive. If importing the drive manually, enable UEFI and set the imported disk as the first boot device. The UTM serial mode should be **Terminal**.

A directly installable native `FadalOS.ipa` cannot be produced in the Linux sandbox because Apple requires an iOS signing certificate, provisioning profile, and device/app entitlements. The UTM bundle is the directly reusable emulator artifact.
