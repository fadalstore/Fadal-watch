# FadalOS Phase 2.18 UTM package

This is a separately versioned P2.18 emulator package. It carries the verified P2.17 network diagnostics release and adds the first native BIOS network-driver status boundary for the RTL8139/FNet path.

The archive contains two independent UTM/QEMU profiles:

- `FadalWatch-BIOS.utm` boots the freestanding 32-bit BIOS kernel with the native RTL8139/FNet stack, Ethernet/ARP/IPv4/UDP/TCP foundations, DHCP path, and the read-only native network status snapshot.
- `FadalWatch-UEFI.utm` boots the independent x86-64 Fadal64 graphical environment with GOP, FDL64 v3 W^X permissions, persistent home, dragon startup splash, floating Files/System/Browser windows, the network dashboard, and `net`/`net summary` diagnostics.

The BIOS native status snapshot reports driver readiness, DHCP state, local IPv4 configuration, gateway, netmask, DNS server, IPv4 receive/drop counters, and firewall mode. The current firewall mode is explicitly `audit-only`. This package does not claim packet-policy enforcement until a policy engine is connected to every native ingress/egress decision and tested with default-deny, loopback, and management exceptions.

The UEFI path remains separate and uses firmware-backed SNP, HTTP, and DNS4 services. Its dashboard reports SNP interface count, IPv4 DHCP state, address, subnet mask, route count, HTTP request/success counters, and firmware capability flags. UEFI HTTP remains bounded plaintext transport in this release; hostname DNS, general Internet access, and HTTPS/TLS are not claimed.

On iOS or Apple Silicon, choose **Emulate**, not **Virtualize**. Import the desired `.utm` profile in UTM. The BIOS and UEFI profiles use separate raw disk images; do not attach the BIOS image as the UEFI ESP or overwrite one image with the other.

`SHA256SUMS` contains checksums of the two raw disk images inside the profiles. From the extracted directory, verify them with `sha256sum -c SHA256SUMS`. The adjacent `FadalOS-Phase2.18-UTM-iOS.zip.sha256` file verifies the archive itself.

The guest runtime remains freestanding and does not depend on Linux or Ubuntu APIs, libc, host shell behavior, or host networking at runtime. Host tools are used only to build, package and test the emulator images.
