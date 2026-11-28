# FadalOS Phase 2.18 progress

Phase 2.18 starts the native network-driver boundary for the independent BIOS kernel without merging it with the UEFI firmware-service path.

## BIOS native status API

The BIOS FNet/RTL8139 stack now exposes a read-only `fnet_network_status` snapshot through `fnet_get_status()`. The snapshot reports whether the native path is ready, whether DHCP is bound, local IPv4 configuration, gateway, netmask, DNS server, IPv4 receive/drop counters, and the current firewall mode.

The current firewall mode is explicitly `FNET_FIREWALL_AUDIT_ONLY`. This is intentional: the existing native path owns Ethernet/ARP/IPv4/UDP/TCP processing, but a policy engine has not yet been wired into every ingress and egress decision. The API therefore reports capability honestly instead of claiming enforcement that does not exist.

BIOS startup diagnostics now report the native status snapshot and RX/drop counters after the FNet self-tests. The existing RTL8139 DMA, ARP, IPv4 routing, UDP, TCP, DHCP, HTTP adapter, and Git transport behavior remains unchanged.

## UEFI separation

The UEFI path remains separate and continues to use its own firmware-backed service contract, System network dashboard, `net`/`net summary` diagnostics, HTTP counters, DNS4 capability audit, and bounded UEFI HTTP service. No BIOS driver object is linked into the UEFI payload.

## Verification

The following checks passed after this milestone:

- `make -C kernel check`
- `make -C kernel/uefi check`
- `make -C kernel/uefi test-network-audit`
- `make -C kernel/uefi test-window-controls`
- `git diff --check`

The BIOS check passed the freestanding independence guard, kernel size guard, QEMU boot smoke test, maximum loader-window test, 64 MiB memory-window tests, and supervisor/user protection checks. UEFI continued to report three SNP interfaces, DHCP station `10.0.2.15` with mask `255.255.255.0`, HTTP availability, DNS4 binding/protocol availability, and successful bounded numeric-IP HTTP fixture requests.

## Next boundary

The next native milestone should add a policy evaluation function that can run at the FNet receive/transmit boundaries in audit mode, with counters for allowed and denied decisions. Enforcement should only be enabled after rules, default-deny behavior, loopback/management exceptions, and regression fixtures are defined and tested.
