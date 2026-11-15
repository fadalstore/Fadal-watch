# FadalOS Phase 2 release checkpoint

- **Checkpoint date:** 2026-11-15
- **Status:** Phase 2 candidate packaged and boot-verified; Phase 1 UTM archives remain frozen.

## Candidate artifact

The separate [Phase 2 UTM package](../FadalOS-Phase2-UTM-iOS.zip) contains a freestanding BIOS profile and a separate x86-64 UEFI profile. It includes the current tested UEFI ESP image and the P2.4 BIOS image. The profiles use separate disk images; the BIOS profile is configured for 128 MiB RAM and legacy `pc` boot. Import on iOS with **Emulate**, not **Virtualize**.

- Archive SHA-256: `7856305f788f26ce93a4d407696615c9b51919224eb5ed7b15b2c8bbdc1365f5`
- Sidecar: [`FadalOS-Phase2-UTM-iOS.zip.sha256`](../FadalOS-Phase2-UTM-iOS.zip.sha256)

Image SHA-256 values recorded inside the ZIP:

- UEFI disk: `9797ed89d293308be350b80decd253807ea4f7478c68888ef35c5044b690f069`
- BIOS disk: `de47f033d94ecd66b0c544979cc77ed8ce74582d7859c6bd7bcc1c1ed95f329d`

The source [package README](PHASE2_PACKAGE_README.md) and [`scripts/package-phase2-utm.sh`](../scripts/package-phase2-utm.sh) describe and reproduce the archive. The legacy `scripts/package-utm.sh` was not used because it rewrites Phase 1 artifacts.

## Verification

- `scripts/validate-repo.sh --uefi` passed, including the freestanding BIOS build/smoke checks, BIOS QEMU 16/32/64 MiB memory-window matrix, UEFI build, and EFI page-protection self-test.
- The complete UEFI QEMU/OVMF matrix passed: page protection, keyboard focus, terminal editing and scrollback, text selection and guest clipboard, eight-entry history and ordering, window controls and Files navigation, all-edge/corner resize and reflow, and persistent-home guest reboot plus host-side raw-ESP checks.
- The terminal-input regression was made state-safe: each edit case now requires its expected result in fresh serial output after the sequence and allows at most three reported attempts. No command-editing assertion was removed.
- The UTM-staged BIOS image booted under QEMU `pc` with 128 MiB RAM and emitted the 64 MiB allocator/bounds proof. The staged UEFI image booted under OVMF and passed focus/keyboard input checks.
- The archive passed `unzip -t`, its external archive checksum passed `sha256sum -c`, and its internal image checksums were verified during packaging.

The Phase 1 archives were not modified; both retain SHA-256 `bd1b433309cce3671f9a206f56629db8d7db8268a0167a378696db705c362fab`.

## Compatibility note and next step

The BIOS kernel payload is 57,273 bytes within its fixed 57,344-byte (112-sector) loader budget, leaving only 71 bytes. The image boots and passes the recorded checks, but future BIOS growth must stop until loader headroom is increased or the payload is reduced through a separately tested change. The next proposed milestone is a bounded BIOS loader-capacity improvement with an explicit image-size ceiling and QEMU negative-control coverage. No new BIOS syscall, UEFI/BIOS unification, or Linux runtime dependency was introduced here.
