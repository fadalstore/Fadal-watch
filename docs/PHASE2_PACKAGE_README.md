# FadalOS Phase 2.6 UTM package

This is a separately versioned P2.6 emulator package. It does not replace or modify the frozen `FadalOS-Phase2-UTM-iOS.zip`, `FadalOS-Phase1-UTM.zip`, or `FadalWatch-UTM-iOS.zip` archives.

The archive contains two independent UTM/QEMU profiles:

- `FadalWatch-BIOS.utm` boots the freestanding 32-bit BIOS kernel. The current P2.6 disk reserves LBAs 1–120 for the kernel load window and places the FAT12 volume at LBA 121. The kernel binary is capped at 61,440 bytes (120 sectors); the boot sector embeds the actual rounded-up sector count and rejects a count above 120 before reading the disk. The memory allocator maps at most 64 MiB and uses only BIOS E820 type-1 pages; its no-E820 fallback remains capped at 16 MiB. The profile is configured for 128 MiB of guest RAM.
- `FadalWatch-UEFI.utm` boots the separately built x86-64 UEFI graphical environment from its own ESP image.

On iOS or Apple Silicon, choose **Emulate**, not **Virtualize**. Import the desired `.utm` profile in UTM. The UEFI and BIOS profiles use separate raw disk images; do not attach the BIOS image as the UEFI ESP or overwrite one image with the other.

The BIOS image passed the repository's freestanding boot/smoke checks, the exact 61,440-byte and one-byte-over host size controls, the QEMU full 120-sector load-window control and 121-sector rejection control, plus QEMU TCG memory-window boots at 16 MiB (negative control), 32 MiB, and 64 MiB. The UEFI image is tested separately under QEMU/OVMF; this package does not combine runtimes.

`SHA256SUMS` contains checksums of the two raw disk images inside the profiles. From the extracted directory, verify them with `sha256sum -c SHA256SUMS` where available. The adjacent `FadalOS-Phase2.6-UTM-iOS.zip.sha256` file verifies the archive itself.

The BIOS loader continues to use the existing single-call INT 13h CHS read and loads at `0x10000`; the 120-sector (61,440-byte) maximum remains inside that segment's 64 KiB window. This is a bounded image layout, not a general BIOS filesystem kernel loader. The guest operating systems do not depend on Linux or Ubuntu at runtime.
