# FadalOS Phase 2.17 UTM package

This is a separately versioned P2.17 emulator package. It carries the verified P2.16 desktop release and adds the first FadalOS network-platform diagnostics layer: live SNP interface inventory, IPv4 DHCP state, IPv4 address and subnet mask, firmware route-table entry count, and bounded HTTP request/success counters.

The archive contains two independent UTM/QEMU profiles:

- `FadalWatch-BIOS.utm` boots the freestanding 32-bit BIOS kernel with its verified loader ceiling, memory-window checks, and supervisor/user protection checks.
- `FadalWatch-UEFI.utm` boots the x86-64 Fadal64 graphical environment with GOP, FDL64 v3 W^X permissions, persistent home, the dragon startup splash, floating Files/System/Browser windows, Terminal, the text Browser, and the P2.17 network dashboard.

In the UEFI desktop, the System window and Terminal commands `net` and `net summary` report UEFI SNP interface count, IPv4 DHCP lease state, IPv4 address, subnet mask, IPv4 route count, HTTP request count, HTTP successful-save count, HTTP binding/protocol state, and DNS4 binding/protocol state. The HTTP counters are application-level service counters; this package does not claim packet-level accounting until a native driver owns the packet path.

The UEFI loader audits DNS4 Service Binding, child creation, OpenProtocol, and Configure when firmware exposes them. Because some OVMF/QEMU firmware can block inside `EFI_DNS4.HostNameToIp` before an asynchronous token is returned, the production boot path deliberately skips that call after the Configure audit and reports a boot-liveness diagnostic. This package does not claim hostname DNS, general Internet access, or HTTPS/TLS.

The verified network audit checks DHCP station configuration, HTTP Service Binding, two sequential numeric-IP plaintext GETs to the bounded local fixture, matching loader/desktop capability reporting, route inventory plumbing, and the bounded HTTP service path. The desktop regression checks focus, Terminal, Files, System, Browser, scrolling, Downloads navigation, and window controls.

On iOS or Apple Silicon, choose **Emulate**, not **Virtualize**. Import the desired `.utm` profile in UTM. The BIOS and UEFI profiles use separate raw disk images; do not attach the BIOS image as the UEFI ESP or overwrite one image with the other.

`SHA256SUMS` contains checksums of the two raw disk images inside the profiles. From the extracted directory, verify them with `sha256sum -c SHA256SUMS`. The adjacent `FadalOS-Phase2.17-UTM-iOS.zip.sha256` file verifies the archive itself.

The guest runtime remains freestanding and does not depend on Linux or Ubuntu APIs, libc, host shell behavior, or host networking at runtime. Host tools are used only to build, package and test the emulator images.
