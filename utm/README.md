# FadalWatch iOS/UTM Package

This package is a UTM QEMU-emulation bundle for the FadalWatch x86_64 UEFI proof-of-concept. It is intended for import into UTM on iOS or macOS. In UTM, choose **Emulate** rather than **Virtualize**, because the guest architecture is x86_64 and iPhone/iPad hardware is ARM64.

The bundle contains a 64 MiB raw ESP image with `EFI/BOOT/BOOTX64.EFI` and `FADAL.KRN`. UTM iOS can import the bundle or import `Data/fadal-uefi.img` as a drive. If importing the drive manually, enable UEFI and set the imported disk as the first boot device.

A directly installable native `FadalWatch.ipa` cannot be produced in the Linux sandbox because Apple requires an iOS signing certificate, provisioning profile, and device/app entitlements. The UTM bundle is the directly reusable emulator artifact. A native iOS wrapper can be built and signed on macOS/Xcode or through AltStore/SideStore after the Apple signing material is supplied.
