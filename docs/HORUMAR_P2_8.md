# Horumarka FadalWatch — P2.8 iyo dib-u-eegista isbeddellada

**Taariikh:** 18 Nofeembar 2026
**Branch la baaray:** `main`
**Commit-ka source-ka P2.8:** `f97422e` — *Reduce BIOS kernel size for loader headroom* (waa la push-gareeyey GitHub). Isbeddellada laga helay bilowga waxay ku jireen `d270cfe`.

## Isbeddellada laga helay kaydka ilaa shaqadii hore

Tan iyo checkpoint-kii dhammaadkii Oktoobar (`88666d1`), kaydka GitHub wuxuu helay horumarro muuqda oo labada boot path ah:

- **UEFI Files iyo terminal:** Files wuxuu helay keyboard navigation iyo liis faylal oo rogmada; terminal-kuna wuxuu ilaaliyaa command-ka marka daaqadda la resize-gareeyo, wuxuuna taageeraa text selection, guest clipboard, iyo taariikh amarro siddeed ah.
- **BIOS TTY iyo stack:** Shift/Caps Lock iyo calaamadaha US keyboard-ka ayaa la hagaajiyey. BIOS TCP receive ring, HTTP response parser, iyo Git pkt-line reader ayaa helay tijaabooyin fragmented-stream iyo xuduudaha saxda ah.
- **Phase 2 architecture iyo memory:** ADR 0001 wuxuu sii hayaa runtime-ka BIOS ee i386, wuxuuna dib u dhigayaa x86-64 BIOS kale. Identity-map/allocator-ka non-PAE waxaa lagu tijaabiyaa 16/32/64 MiB iyadoo E820 la raacayo.
- **BIOS loader iyo release:** Xadka kernel loader-ka waxaa laga qaaday 112 ilaa 120 sectors; tirada sectors-ka dhabta ah ayaa boot sector-ku kaydiyaa. UTM archive P2.6 oo gooni ah ayaa la diyaariyey iyadoo Phase 1 iyo P2.5 aan la beddelin.
- **Ilaalinta xusuusta P2.7:** BIOS `.text` iyo `.rodata` waa supervisor read-only iyadoo `CR0.WP` shaqaynayo. #PF handler-ku wuxuu soo celiyaa oo keliya ring-0 store-ka tijaabada ee si sax ah loo aqoonsaday; faults-ka kale waa fail-closed. Process mappings iyo syscall copyout sidoo kale waa la hubiyaa.

## Shaqada P2.8 ee la qabtay

P2.7 kernel binary-gu wuxuu ahaa **61,305/61,440 bytes**, taas oo uga tagtay 135 bytes oo keliya. Halkii loader-ka CHS iyo disk layout-ka khatar loogu kordhin lahaa, waxaan default BIOS compiler flag-ka ka beddelay `-O2` una beddelay `-Os` gudaha `kernel/Makefile`. Waa isbeddel source/build oo keliya: 120-sector ceiling, boot-sector guard, FAT12 start-ka LBA 121, syscall ABI, iyo UTM profiles lama beddelin.

Natiijada build-ku waa **47,609 bytes**: waxaa la dhimay **13,696 bytes**, waxaana hadda bannaan **13,831 bytes** gudaha xadka 61,440-byte. Boot sector-ku wuxuu geliyey **93 sectors** oo xogta kernel-ka ah, halka loader-ku weli aqbalo ugu badnaan 120.

## Xaqiijinta

Labadan amar ayaa gudbay:

```sh
make -C kernel check
scripts/validate-repo.sh --uefi
```

Tijaabooyinku waxay xaqiijiyeen BIOS smoke/ABI, size guard-ka xadka saxda ah iyo hal-byte-over, QEMU boot-ka 120-sector, diidmada tirada 121 ka hor kernel entry, memory-window-ka 16/32/64 MiB, CR0.WP recovery iyo mapping checks, iyo UEFI loader/payload/page-protection build. Intii validation-ka la diyaarinayey waxaa sandbox-ka lagu rakibay QEMU, GNU-EFI iyo OVMF; waxba laguma rakibin ama laguma darin guest-ka FadalOS.

## UTM ZIP-ga P2.8

Waxaa la sameeyey [FadalOS-Phase2.8-UTM-iOS.zip](../FadalOS-Phase2.8-UTM-iOS.zip), cabbirkiisuna waa **159,288 bytes**. SHA-256-ga archive-ka waa `b2f421d86f641a9bbec185cc6a5189cbff4ce8d7b5246bec3eb6af0bd2ac151a`; checksum-ku wuxuu kaloo ku jiraa [sidecar-ka](../FadalOS-Phase2.8-UTM-iOS.zip.sha256).

Hashes-ka labada disk ee gudaha ZIP-ga waa:

- UEFI ESP: `14d518b61d7b8deb4af45eb9244d552c565a799996cd5f94dc65207eb5dbe4a2`
- BIOS disk: `e7e559013566b64090e1e3580adb4ac2463ef8ddd949311557606cbe9da68b88`

`unzip -t`, checksum-ka sidecar-ka, internal `SHA256SUMS`, iyo byte-for-byte isbarbardhigga images-ka gudaha waa gudbeen. BIOS image-ka ZIP-ga laga soo saaray wuxuu QEMU ku gaaray `FADAL KERNEL ONLINE` iyadoo la adeegsanayo 128 MiB RAM. UEFI ESP-ga la soo saaray wuxuu OVMF ku gudbay focus/window-controls iyo Files/System/Browser; EFI page-protection self-test gooni ahna wuxuu qabtay NX/read-only faults-kii la filayey. Hash-yada archives-kii hore isma beddelin: P2.6 `220d3f185298878a4fc05a076a383dc18f31020ef0f4f098e2b6b429660e3825`, P2.5 `7856305f788f26ce93a4d407696615c9b51919224eb5ed7b15b2c8bbdc1365f5`, iyo Phase 1 `bd1b433309cce3671f9a206f56629db8d7db8268a0167a378696db705c362fab`.

P2.6 iyo Phase 1 waa la ilaaliyey; ZIP-ga cusub waa nooc gooni ah.

## Shaqada xigta

**P2.9 — ring-3 write-to-code negative control:** naqshadee tijaabo dhab ah oo user mode ah oo isku dayda in ay wax ku qorto bogga code-ka ee read-only. Recovery-gu waa inuu ku ekaadaa hal probe oo si gaar ah loo aqoonsaday—user privilege, CR2, write/protection bits, faulting EIP, iyo resume EIP—iyadoo faults-ka kale oo dhan ay sii ahaanayaan fail-closed. Tijaabadu waa inay sidoo kale xaqiijisaa in byte-ga code-ku aanu isbeddelin.

## Qodob UEFI ah oo aan la isku darin

Kaydka `main` ee hadda jira wuxuu Browser-ka UEFI ku tiirsan yahay EFI HTTP protocol; qoraalka handoff-ku si cad ayuu u sheegayaa in OVMF-ga caadiga ahi aanu bixin adeeggaas. Workspace-kii session-kii hore wuxuu lahaa qaybo TCP4 fallback ah, balse nuqulka la soo celiyey waa qayb ahaan: `tcp_http.c` wuu jiraa halka `net.h` iyo `tcp_http.h` aysan jirin, `payload.c`-guna wuu ka duwanaa nooca `main` ee hadda leh terminal selection/history iyo Files scrolling. Sidaas darteed code-kaas si toos ah looguma dul-qorin source-ka cusub. Haddii UEFI fallback-ka la sii wado, waa in si ula kac ah loogu port-gareeyo ABI-ga iyo payload-ka cusub, kadibna QEMU/OVMF live HTTP test lagu xaqiijiyaa.
