# FadalOS Phase 2 UTM package

This is a separate Phase 2 emulator package. It does not replace or modify the frozen `FadalOS-Phase1-UTM.zip` or `FadalWatch-UTM-iOS.zip` archives.

The archive contains two independent UTM/QEMU profiles:

- `FadalWatch-BIOS.utm` boots the freestanding 32-bit BIOS kernel. The Phase 2 image maps up to 64 MiB with non-PAE 4 KiB paging and uses only E820 type-1 memory. Its no-E820 fallback remains capped at 16 MiB. The profile is configured for 128 MiB of guest RAM; it does not merge the BIOS runtime with UEFI.
- `FadalWatch-UEFI.utm` boots the separately built x86-64 UEFI graphical environment from its own ESP image.

On iOS or Apple Silicon, choose **Emulate**, not **Virtualize**. Import the desired `.utm` profile in UTM. The UEFI and BIOS profiles use separate raw disk images; do not attach the BIOS image as the UEFI ESP or overwrite one image with the other.

The BIOS image passed the repository's original freestanding boot/smoke checks and the QEMU TCG memory-window matrix at 16 MiB (negative control), 32 MiB, and 64 MiB. The UEFI build passed the QEMU/OVMF regression matrix for page protection, keyboard/terminal editing, text selection and history, window controls and resizing, and persistent-home reboot/artifact checks.

`SHA256SUMS` contains the checksums of the two raw disk images inside the profiles. From the extracted directory, verify them with `sha256sum -c SHA256SUMS` where `sha256sum` is available. The adjacent `FadalOS-Phase2-UTM-iOS.zip.sha256` file verifies the archive itself.

Known limitation: the BIOS kernel binary uses the fixed 112-sector loader budget and this release leaves only 71 bytes unused. Future BIOS changes must confirm the kernel payload remains within that limit and should first create additional budget headroom. The guest operating systems do not depend on Linux or Ubuntu at runtime.