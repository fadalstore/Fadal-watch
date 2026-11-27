# FadalOS Phase 2.16 progress

Phase 2.16 completes the next UEFI desktop presentation milestone without changing the independent BIOS and UEFI boot paths.

## Delivered

- Added a visible procedural dragon startup splash with FADAL OS branding while the UEFI desktop initializes.
- Added shared floating-window chrome for Files, System, and Browser, including title bars, shadows, focus styling, and computer-like separate windows.
- Added minimize, maximize/restore, and close controls for desktop applications.
- Added taskbar buttons that restore minimized applications.
- Preserved Terminal controls, focus/input behavior, Files scrolling and navigation, Downloads navigation, System reporting, and Browser URL input.
- Kept the existing UEFI framebuffer, PS/2 input, GOP, UART, and UEFI service based implementation freestanding. No Linux/Ubuntu runtime API was added.

## Verification

The following checks passed after the UI changes:

- `make -C kernel/uefi check`
- `make -C kernel/uefi test-window-controls`
- `make -C kernel/uefi test-network-audit`
- `make -C kernel check`
- `git diff --check`

The window regression covers Terminal focus/input, maximize/restore/minimize/close, Start > Files, Start > System, Start > Browser, Browser URL input, Files scrolling/navigation, Downloads navigation, window controls, and focus behavior.

The BIOS release check passed the freestanding independence guard, kernel size checks, boot smoke test, maximum 120-sector loader-window test, 64 MiB memory-window tests, and supervisor/user write-protection checks.

The UEFI network audit passed loader capability reporting, DHCP station configuration, HTTP binding, DNS4 service-binding and protocol-instance creation, two sequential numeric-IP plaintext HTTP fixture requests, and matching loader/desktop `net` reporting. It intentionally does not claim hostname DNS, general Internet access, or HTTPS/TLS.

Regression screenshots are kept at:

- `kernel/uefi/out/window-controls.ppm`
- `kernel/uefi/out/keyboard-focus.ppm`
- `kernel/uefi/out/network-audit-system.ppm`

## Runtime independence

FadalOS still has two separate freestanding paths: the 32-bit BIOS kernel under `kernel/` and the x86-64 UEFI/Fadal64 payload under `kernel/uefi/`. Linux/Ubuntu tools are used only on the host to compile, run QEMU/OVMF tests, and package the UTM artifact; the guest runtime does not depend on Linux, Ubuntu, libc, host shell behavior, or host networking.

## Release artifact

The versioned UTM package is `FadalOS-Phase2.16-UTM-iOS.zip`. The previous P2.15 archive remains unchanged and is not overwritten.
