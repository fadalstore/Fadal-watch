# FadalOS Phase 2.14 UTM package

This is a separately versioned P2.14 emulator package. It preserves the earlier BIOS and UEFI package line while adding the refreshed FadalOS UEFI desktop presentation.

The archive contains two independent UTM/QEMU profiles:

- `FadalWatch-BIOS.utm` boots the freestanding 32-bit BIOS kernel. Its image uses the verified loader ceiling, FAT12 layout, memory-window checks, and supervisor/user protection checks. The profile is configured for 128 MiB guest RAM.
- `FadalWatch-UEFI.utm` boots the x86-64 Fadal64 graphical environment from a separate ESP image. The UEFI loader uses GOP, FDL64 v3 W^X permissions, persistent home, desktop, terminal, Files, System and the text browser. The desktop includes a procedural dragon emblem, refreshed top bar, and visible launcher cards for Terminal, Files, System and Browser.

On iOS or Apple Silicon, choose **Emulate**, not **Virtualize**. Import the desired `.utm` profile in UTM. The BIOS and UEFI profiles use separate raw disk images; do not attach the BIOS image as the UEFI ESP or overwrite one image with the other.

The UEFI interaction regression verifies desktop focus, terminal input, window maximize/minimize/close behavior, Start-menu relaunch, Files navigation and scrolling, System status, Browser URL input, Downloads navigation, and held-contact click behavior. The UEFI network audit and bounded local HTTP/text-browser path remain available; general Internet access and HTTPS/TLS are not claimed by this package.

`SHA256SUMS` contains checksums of the two raw disk images inside the profiles. From the extracted directory, verify them with `sha256sum -c SHA256SUMS`. The adjacent `FadalOS-Phase2.14-UTM-iOS.zip.sha256` file verifies the archive itself.

The guest runtime remains freestanding and does not depend on Linux or Ubuntu APIs, libc, host shell behavior, or host networking at runtime. Host tools are used only to build, package and test the emulator images.
