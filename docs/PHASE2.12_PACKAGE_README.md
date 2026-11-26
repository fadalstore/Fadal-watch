# FadalOS Phase 2.12 UTM package

This is a separately versioned P2.12 emulator package. It does not replace or modify the frozen Phase 1, P2.6, P2.8, or P2.10 archives.

The archive contains two independent UTM/QEMU profiles:

- `FadalWatch-BIOS.utm` boots the freestanding 32-bit BIOS kernel. Its current image uses the 120-sector loader ceiling, the LBA-121 FAT12 layout, and the verified 16/32/64 MiB memory-window and protection checks. The profile is configured for 128 MiB guest RAM.
- `FadalWatch-UEFI.utm` boots the x86-64 Fadal64 graphical environment from a separate ESP image. The UEFI loader uses GOP, FDL64 v3 W^X permissions, persistent home, desktop, terminal, Files, System and the text browser.

On iOS or Apple Silicon, choose **Emulate**, not **Virtualize**. Import the desired `.utm` profile in UTM. The BIOS and UEFI profiles use separate raw disk images; do not attach the BIOS image as the UEFI ESP or overwrite one image with the other.

The P2.12 UEFI network audit creates an EFI DNS4 child through the available DNS4 Service Binding when a direct protocol instance is absent. It opens and configures the child using DHCP/default DNS settings and submits a bounded `HostNameToIp` probe. QEMU/OVMF verified child creation, `OpenProtocol`, Configure and query submission.

DNS/hostname is **not yet verified**. The hostname fixture request still fails at `EFI_HTTP.Request` with `stage=5 status=Not Found HTTP=0`, and the P2.12 packet capture contains no port-53 traffic. General Internet access and HTTPS/TLS are not implemented or verified. The numeric-IP HTTP fixture is local and bounded; it is not evidence of general browsing.

`SHA256SUMS` contains checksums of the two raw disk images inside the profiles. From the extracted directory, verify them with `sha256sum -c SHA256SUMS`. The adjacent `FadalOS-Phase2.12-UTM-iOS.zip.sha256` file verifies the archive itself.

The guest runtime remains freestanding and does not depend on Linux or Ubuntu APIs, libc, host shell behavior, or host networking at runtime. Host tools are used only to build, package and test the emulator images.
