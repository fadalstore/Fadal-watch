# FadalOS Phase 2.17 progress

Phase 2.17 begins the FadalOS network-platform foundation. This milestone does not replace the existing UEFI firmware networking with a host Linux service; instead, it makes the firmware capabilities visible through a typed FadalOS service contract and the native UEFI desktop.

## Network service model

The UEFI loader now passes the following live network inventory to the Fadal64 payload:

- The number of exposed UEFI Simple Network Protocol interfaces.
- IPv4 DHCP lease state.
- The active IPv4 station address.
- The active IPv4 subnet mask.
- The IPv4 route-table entry count exposed by the firmware configuration protocol.
- Bounded HTTP request and successful-save counters maintained by the FadalOS service bridge.
- Existing HTTP binding, HTTP protocol, DNS4 binding, DNS4 protocol, persistent-home, and simple-network capability flags.

When the bounded HTTP client prepares DHCP, the service object is updated in place so later desktop views can see the acquired lease instead of only the boot-time state.

## Desktop work started

The System floating window now includes an initial network dashboard showing SNP interface count, IPv4 route count, DHCP state, address, subnet mask, and bounded HTTP request/success counters alongside the existing HTTP and DNS4 capability report. The Terminal accepts both `net` and `net summary`, and the existing Browser behavior remains intact.

This is an inventory and diagnostics foundation, not a claim that all radios, towers, satellites, mobile devices, or arbitrary networks are supported. Each future hardware family requires an explicit driver and protocol implementation, and network/security operations must remain authorized.

## Verification

The following checks passed after the Phase 2.17 changes:

- `make -C kernel/uefi check`
- `make -C kernel/uefi test-window-controls`
- `make -C kernel/uefi test-network-audit`
- `git diff --check`

The window regression still covers Terminal focus/input, window controls, Files, System, Browser, scrolling, Downloads navigation, and URL input. The network audit still reports three SNP interfaces, DHCP station `10.0.2.15` with mask `255.255.255.0` under QEMU/OVMF, HTTP service availability, DNS4 binding/protocol availability, and successful bounded numeric-IP HTTP fixture requests.

## Runtime independence

The implementation remains freestanding in the guest. Linux/Ubuntu tools are used only on the host to compile, run QEMU/OVMF checks, and package images. The FadalOS runtime does not call Linux, Ubuntu, libc, host shell, or host networking APIs.

## Next planned layers

The next network milestones should add a stable FadalOS interface/route query API over multiple adapters, true packet counters when a native driver owns the data path, and firewall policy boundaries. Those should be followed by bounded native transport services and explicit USB/Wi-Fi/Bluetooth/SDR driver work where hardware and protocol specifications are available. The current HTTP counters are application-level service counters, not packet-level accounting. Each layer will receive a regression target before entering a release archive.
