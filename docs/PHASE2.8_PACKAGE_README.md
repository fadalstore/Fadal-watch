# FadalOS Phase 2.8 — xirmada UTM

Tani waa xirmo UTM oo gooni ah, nooca **P2.8**. Ma beddelayso mana cusboonaysiinayso `FadalOS-Phase2-UTM-iOS.zip`, `FadalOS-Phase2.6-UTM-iOS.zip`, ama ZIP-yada Phase 1.

Xirmadu waxay leedahay laba profile oo kala madax-bannaan:

- **FadalWatch-BIOS.utm** wuxuu kiciyaa kernel-ka freestanding ee 32-bit BIOS. P2.8 wuxuu kernel-ka ku dhisaa `-Os`; cabbirka binary-ga waa 47,609 bytes, 93 sectors ayaa loo baahan yahay, loader-kuna weli wuxuu aqbalaa ugu badnaan 120 sectors (61,440 bytes). FAT12 wuxuu ka bilaabmaa LBA 121. BIOS memory allocator-ku wuxuu khariidadeeyaa ugu badnaan 64 MiB oo RAM ah, wuxuuna ku tiirsan yahay bogagga E820 ee usable-ka ah; haddii E820 la waayo xadka fallback-ku waa 16 MiB. Profile-ku wuxuu u qoondeeyaa 128 MiB RAM guest-ka.
- **FadalWatch-UEFI.utm** wuxuu kiciyaa desktop-ka garaafyada x86-64 ee UEFI, isagoo adeegsanaya ESP u gaar ah. BIOS image-ka laguma beddeli karo ESP-ga UEFI-ga.

UTM ee iOS ama Apple Silicon, dooro **Emulate**, ha dooran **Virtualize**. Soo geli profile-ka BIOS ama UEFI ee aad rabto; labaduba waxay ku jiraan ZIP-kan.

`SHA256SUMS` wuxuu hubiyaa labada disk image ee gudaha profiles-ka. Marka ZIP-ka la soo saaro, ka orod `sha256sum -c SHA256SUMS`. Checksum-ka ZIP-ka oo dhan wuxuu ku jiraa faylka dhinaciisa ah `FadalOS-Phase2.8-UTM-iOS.zip.sha256`.

BIOS image-ku wuxuu gudbay `make -C kernel check`, oo ay ku jiraan boot smoke test-ka QEMU, 120-sector maximum load, diidmada 121-sector, iyo memory-window tests-ka 16/32/64 MiB. UEFI ESP-ga package-ka waxaa OVMF hoostiisa lagu tijaabiyey keyboard focus iyo window controls, Files/System/Browser launch, iyo taabasho/mouse edge behavior. EFI page-protection test gooni ah, oo wadaaga page-table walker-ka, wuxuu xaqiijiyey NX iyo read-only fault recovery. Package builder-ku wuxuu dib u hubiyaa ZIP integrity-ga, labada image checksum, iyo in images-ka gudaha ay byte-for-byte ula mid yihiin kuwa la tijaabiyey.

Tani waa xirmo tijaabo ah oo source-ka FadalOS ah, mana sheeganayso in BIOS iyo UEFI ay hal kernel wadaagaan. Guest-ku kuma tiirsana Linux ama Ubuntu runtime ahaan. UEFI Browser-ku weli wuxuu u baahan yahay adeegga EFI HTTP ee firmware-ka; HTTPS/TLS iyo DNS fallback-ku kuma jiraan xirmadan.
