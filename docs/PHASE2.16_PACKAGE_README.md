# FadalOS Phase 2.16 UTM package

This is a separately versioned P2.16 emulator package. It carries the verified P2.15 BIOS/UEFI release and adds the UEFI dragon startup splash plus floating desktop windows and window controls.

The archive contains two independent UTM/QEMU profiles:

- `FadalWatch-BIOS.utm` boots the freestanding 32-bit BIOS kernel with its verified loader ceiling, memory-window checks, and supervisor/user protection checks.
- `FadalWatch-UEFI.utm` boots the x86-64 Fadal64 graphical environment with GOP, FDL64 v3 W^X permissions, persistent home, the dragon startup splash, shared floating-window chrome, title bars, shadows, taskbar restore behavior, minimize/maximize/restore/close controls, Terminal, Files, System, and the text Browser.

The UEFI loader audits DNS4 Service Binding, child creation, OpenProtocol, and Configure when firmware exposes them. Because some OVMF/QEMU firmware can block inside `EFI_DNS4.HostNameToIp` before an asynchronous token is returned, the production boot path deliberately skips that call after the Configure audit and reports a boot-liveness diagnostic. This package does not claim hostname DNS, general Internet access, or HTTPS/TLS.

The verified network audit checks DHCP station configuration, HTTP Service Binding, two sequential numeric-IP plaintext GETs to the bounded local fixture, and matching loader/desktop capability reporting. The desktop regression checks focus, Terminal, Files, System, Browser, scrolling, Downloads navigation, and window controls.

On iOS or Apple Silicon, choose **Emulate**, not **Virtualize**. Import the desired `.utm` profile in UTM. The BIOS and UEFI profiles use separate raw disk images; do not attach the BIOS image as the UEFI ESP or overwrite one image with the other.

`SHA256SUMS` contains checksums of the two raw disk images inside the profiles. From the extracted directory, verify them with `sha256sum -c SHA256SUMS`. The adjacent `FadalOS-Phase2.16-UTM-iOS.zip.sha256` file verifies the archive itself.

The guest runtime remains freestanding and does not depend on Linux or Ubuntu APIs, libc, host shell behavior, or host networking at runtime. Host tools are used only to build, package and test the emulator images.
