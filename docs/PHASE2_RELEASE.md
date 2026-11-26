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

The BIOS kernel payload is 57,273 bytes within its fixed 57,344-byte (112-sector) loader budget, leaving only 71 bytes. The image boots and passes the recorded checks, but future BIOS growth must stop until loader headroom is increased or the payload is reduced through a separately tested change. At the time of this P2.5 checkpoint, the next proposed milestone was a bounded BIOS loader-capacity improvement. No new BIOS syscall, UEFI/BIOS unification, or Linux runtime dependency was introduced here.

## P2.6 source-build follow-up — 2026-11-16

P2.6 completed the bounded loader-capacity work: current BIOS builds reserve at most 120 sectors (61,440 bytes), embed the rounded-up actual sector count, and reject zero or 121 before INT 13h. The raw-disk FAT12 volume now starts at LBA 121 for newly built images. The BIOS test suite passed its exact-size/one-byte-over checks, maximum 120-sector QEMU boot, 121-sector rejection control, existing BIOS smoke test, and 16/32/64 MiB memory matrix. The P2.5 ZIP above and both Phase 1 UTM archives remain unchanged with their recorded checksums and older 112-sector/LBA-113 disk layout.

The separately versioned [P2.6 UTM package](../FadalOS-Phase2.6-UTM-iOS.zip) is 163,033 bytes and has SHA-256 `220d3f185298878a4fc05a076a383dc18f31020ef0f4f098e2b6b429660e3825`; its sidecar is [`FadalOS-Phase2.6-UTM-iOS.zip.sha256`](../FadalOS-Phase2.6-UTM-iOS.zip.sha256). Internal image hashes are:

- UEFI ESP: `537cee4bb9dbb7ee16993d92e83873b75a2c77245b57833ace047c3c8ffd86da`
- BIOS disk: `37e10c0376881d389df6364c435271d0adbcdfb01ada3160656d3f6e210aa77f`

The package passed `unzip -t`, external archive checksum verification, and its internal per-image hashes. Both images extracted from the ZIP were boot-tested: the BIOS profile under QEMU `pc` with 128 MiB RAM reached `FADAL KERNEL ONLINE` and passed the 64 MiB allocator trace; the UEFI ESP under OVMF passed the keyboard-focus regression. The full UEFI QEMU/OVMF matrix passed, including page protection, terminal input/selection/history, window controls/resizing, and persistent-home checks. The original Phase 1 ZIP aliases still have SHA-256 `bd1b433309cce3671f9a206f56629db8d7db8268a0167a378696db705c362fab`; the P2.5 ZIP retains SHA-256 `7856305f788f26ce93a4d407696615c9b51919224eb5ed7b15b2c8bbdc1365f5`.


## P2.8 — source iyo UTM package (18 Nofeembar 2026)

[UTM ZIP-ga P2.8](../FadalOS-Phase2.8-UTM-iOS.zip) waa 159,288 bytes, SHA-256-giisuna waa `b2f421d86f641a9bbec185cc6a5189cbff4ce8d7b5246bec3eb6af0bd2ac151a`. Sidecar-ku waa [`FadalOS-Phase2.8-UTM-iOS.zip.sha256`](../FadalOS-Phase2.8-UTM-iOS.zip.sha256). Hashes-ka images-ka ku jira waa:

- UEFI ESP: `14d518b61d7b8deb4af45eb9244d552c565a799996cd5f94dc65207eb5dbe4a2`
- BIOS disk: `e7e559013566b64090e1e3580adb4ac2463ef8ddd949311557606cbe9da68b88`

`make -C kernel check` iyo `scripts/validate-repo.sh --uefi` way gudbeen. `unzip -t`, sidecar checksum, gudaha `SHA256SUMS`, iyo byte equality-ga images-ka ZIP-ga ku jira waa la xaqiijiyey. BIOS disk-ka la soo saaray wuxuu QEMU ku boot-gareeyey 128 MiB RAM; UEFI disk-ka la soo saaray wuxuu OVMF ku gudbay keyboard focus, Files/System/Browser, mouse/window controls. EFI page-protection test-ka goonida ahi wuxuu qabtay NX iyo read-only page faults-kii la filayey.

Commit-ka source-ka `f97422e` waa la push-gareeyey `main`-ka GitHub. P2.5 (`7856305f788f26ce93a4d407696615c9b51919224eb5ed7b15b2c8bbdc1365f5`), P2.6 (`220d3f185298878a4fc05a076a383dc18f31020ef0f4f098e2b6b429660e3825`), iyo Phase 1 (`bd1b433309cce3671f9a206f56629db8d7db8268a0167a378696db705c362fab`) archives-koodii isma beddelin.


## P2.10 — UTM candidate package (22 Nofeembar 2026)

[UTM ZIP-ga P2.10](../FadalOS-Phase2.10-UTM-iOS.zip) waa 161,481 bytes, SHA-256-giisuna waa `87ec98d8163da8ae0653fb7f615473452ad4b152a1617d21174bb94482b10cc9`. Sidecar-ku waa [`FadalOS-Phase2.10-UTM-iOS.zip.sha256`](../FadalOS-Phase2.10-UTM-iOS.zip.sha256). Disk-image hashes-ka ku jira `SHA256SUMS` waa:

- UEFI ESP: `c99208003db03f4aa43b0718d3b7cfbc11893425fac054a1310e72dd9e1c3127`
- BIOS disk: `f4a8ee750ec4868e51f4423ebed6987544d04e433921be1d35d1de304c4635d7`

`make -C kernel check`, `make -C kernel/uefi clean check payload`, iyo `scripts/validate-repo.sh --uefi` way gudbeen. `scripts/package-phase2-utm.sh` wuxuu xaqiijiyey ZIP integrity, gudaha image hashes, iyo byte equality-ga labada image; sidecar checksum-kuna wuu gudbay. BIOS image-ka la baakadeeyey wuxuu la mid yahay raw disk image-kii QEMU smoke test-ka gudbay. UEFI image-ka ZIP-ga laga soo saaray ayaa OVMF hoostiisa boot-gareeyey; `e1000` network audit-ku wuxuu xaqiijiyey DHCP `10.0.2.15/24` iyo GET-ga plaintext ee fixture maxalliga ah, waxaana la kormeeray System framebuffer-ka.

Tani waa **candidate package** oo leh caddayn networking xaddidan. Hostname `10.0.2.100.nip.io` probe-kiisu wuu fashilmay, sababta DNS ama isku-xir si gooni ah looma kala saarin; DNS/hostname waa unverified. Internet guud iyo HTTPS/TLS lama tijaabin. Phase 1, P2.5, P2.6, iyo P2.8 archives-koodii waa la ilaaliyey, checksums-kooduna isma beddelin: Phase 1 `bd1b433309cce3671f9a206f56629db8d7db8268a0167a378696db705c362fab`, P2.5 `7856305f788f26ce93a4d407696615c9b51919224eb5ed7b15b2c8bbdc1365f5`, P2.6 `220d3f185298878a4fc05a076a383dc18f31020ef0f4f098e2b6b429660e3825`, iyo P2.8 `b2f421d86f641a9bbec185cc6a5189cbff4ce8d7b5246bec3eb6af0bd2ac151a`.


## P2.12 — DNS4 child-path UTM candidate package (26 Nofeembar 2026)

[UTM ZIP-ga P2.12](../FadalOS-Phase2.12-UTM-iOS.zip) waa 162,311 bytes, SHA-256-giisuna waa `7853f7de0890328d1ae71f0be19d9383331ca6b5f1831fbd1e216a066957111e`. Sidecar-ku waa [`FadalOS-Phase2.12-UTM-iOS.zip.sha256`](../FadalOS-Phase2.12-UTM-iOS.zip.sha256). Disk-image hashes-ka ku jira `SHA256SUMS` waa:

- UEFI ESP: `8ced2690e0c391645ed3e1efdae742c141eec851e0e57411895a438225d9f559`
- BIOS disk: `f4a8ee750ec4868e51f4423ebed6987544d04e433921be1d35d1de304c4635d7`

P2.12 source commit-ka `60e48ce` wuxuu ku daray DNS4 Service Binding child creation, `OpenProtocol`, Configure, iyo bounded `HostNameToIp` submission marka direct DNS4 instance uusan jirin. BIOS iyo UEFI images-ka la baakadeeyey waa kuwa build/test-ka la xaqiijiyey. `unzip -t`, sidecar SHA-256, internal `SHA256SUMS`, iyo byte equality-ga labada image way gudbeen. README-ga gaarka ah ee package-ku waa [`docs/PHASE2.12_PACKAGE_README.md`](PHASE2.12_PACKAGE_README.md).

QEMU/OVMF audit-ku wuxuu xaqiijiyey DHCP `10.0.2.15/24`, DNS4 child creation/OpenProtocol/Configure/query submission, iyo numeric-IP HTTP fixture GET. Hostname/DNS wali **lama xaqiijin**: probe-ku wuxuu ku fashilmay `EFI_HTTP.Request` (`stage=5 status=Not Found HTTP=0`), fixture-kuna ma helin hostname request, P2.12 pcap-kuna wuxuu lahaa eber port-53 packets. Internet guud iyo HTTPS/TLS lama tijaabin ama lama hirgelin. Phase 1, P2.5, P2.6, P2.8, iyo P2.10 archives-koodii iyo checksums-koodii lama taaban.
