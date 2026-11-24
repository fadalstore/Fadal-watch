# FadalOS Phase 2.10 — xirmada tijaabada UTM

Tani waa UTM ZIP cusub oo gooni ah, nooca **P2.10**. Ma beddelayso mana cusboonaysiinayso `FadalOS-Phase2-UTM-iOS.zip`, `FadalOS-Phase2.6-UTM-iOS.zip`, `FadalOS-Phase2.8-UTM-iOS.zip`, ama archives-ka Phase 1.

Xirmadu waxay leedahay laba profile oo madax-bannaan:

- **FadalWatch-BIOS.utm** wuxuu kiciyaa kernel-ka freestanding ee 32-bit BIOS. Disk image-ku wuxuu adeegsadaa FAT12 layout-ka cusub ee LBA 121, wuxuuna ku xaddidan yahay 120 sector (61,440 bytes) oo kernel ah. Profile-ku wuxuu u qoondeeyaa 128 MiB RAM guest-ka.
- **FadalWatch-UEFI.utm** wuxuu kiciyaa desktop-ka x86-64 UEFI ee ku jira ESP u gaar ah. BIOS disk image-ka laguma beddeli karo ESP-ga UEFI-ga.

UTM ee iOS ama Apple Silicon, dooro **Emulate**, ha dooran **Virtualize**. Labada profile waxay ku jiraan ZIP-kan.

`SHA256SUMS` wuxuu hubiyaa labada disk image ee gudaha profiles-ka. Marka ZIP-ka la soo saaro, ka orod `sha256sum -c SHA256SUMS`. Checksum-ka ZIP-ka oo dhan wuxuu ku jiraa sidecar-ka `FadalOS-Phase2.10-UTM-iOS.zip.sha256`.

BIOS build-ku wuxuu gudbay `scripts/validate-repo.sh --uefi`, oo ay ku jiraan BIOS freestanding guard, smoke test-ka QEMU, loader size guards iyo memory-window checks. UEFI desktop-ka waxaa QEMU/OVMF hoostiisa lagu boot-gareeyey, waxaana la kormeeray System panel-ka framebuffer-ka.

## Xaaladda networking ee la xaqiijiyey

QEMU/OVMF test-ka `e1000` wuxuu helay DHCP station `10.0.2.15/24`. EFI_HTTP si toos ah loogama helin `LocateProtocol`, laakiin HTTP Service Binding ayaa jiray; `CreateChild` iyo `OpenProtocol` way guulaysteen. `make -C kernel/uefi test-network-audit` wuxuu xaqiijiyey plaintext HTTP GET ku socda numeric IP oo ku wajahan fixture maxalli ah oo xaddidan, wuxuuna kaydiyey/akhriyey marker-ka `FADAL_UEFI_HTTP_FIXTURE_OK`.

Tijaabadani ma caddaynayso internet guud. Hostname `10.0.2.100.nip.io` oo loo adeegsaday fixture-ka wuu fashilmay, fixture-kuna request ma helin; sababtu si gooni ah ugama cadda DNS iyo connection failure, sidaas darteed DNS/hostname waa **lama xaqiijin**. Fadal64 ma laha native DNS client ama fallback. HTTPS/TLS, certificate validation, iyo secure web browsing kuma jiraan xirmadan.

Tani waa xirmo tijaabo ah oo source-ka FadalOS ah, mana sheeganayso in BIOS iyo UEFI ay hal kernel wadaagaan. Guest-ku kuma tiirsana Linux ama Ubuntu runtime ahaan.
