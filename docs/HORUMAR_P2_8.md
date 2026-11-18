# Horumarka FadalWatch — P2.8 iyo dib-u-eegista isbeddellada

**Taariikh:** 18 Nofeembar 2026
**Branch la baaray:** `main`
**Commit-kii ugu dambeeyey:** `d270cfe` — *Add BIOS kernel write protection*

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

**Wax UTM archive ah dib looma baakadeeyn.** P2.6 iyo Phase 1 waxay ahaanayaan artifacts taariikhi ah oo frozen ah; isbeddelkan waa source checkpoint oo keliya.

## Shaqada xigta

**P2.9 — ring-3 write-to-code negative control:** naqshadee tijaabo dhab ah oo user mode ah oo isku dayda in ay wax ku qorto bogga code-ka ee read-only. Recovery-gu waa inuu ku ekaadaa hal probe oo si gaar ah loo aqoonsaday—user privilege, CR2, write/protection bits, faulting EIP, iyo resume EIP—iyadoo faults-ka kale oo dhan ay sii ahaanayaan fail-closed. Tijaabadu waa inay sidoo kale xaqiijisaa in byte-ga code-ku aanu isbeddelin.

## Qodob UEFI ah oo aan la isku darin

Kaydka `main` ee hadda jira wuxuu Browser-ka UEFI ku tiirsan yahay EFI HTTP protocol; qoraalka handoff-ku si cad ayuu u sheegayaa in OVMF-ga caadiga ahi aanu bixin adeeggaas. Workspace-kii session-kii hore wuxuu lahaa qaybo TCP4 fallback ah, balse nuqulka la soo celiyey waa qayb ahaan: `tcp_http.c` wuu jiraa halka `net.h` iyo `tcp_http.h` aysan jirin, `payload.c`-guna wuu ka duwanaa nooca `main` ee hadda leh terminal selection/history iyo Files scrolling. Sidaas darteed code-kaas si toos ah looguma dul-qorin source-ka cusub. Haddii UEFI fallback-ka la sii wado, waa in si ula kac ah loogu port-gareeyo ABI-ga iyo payload-ka cusub, kadibna QEMU/OVMF live HTTP test lagu xaqiijiyaa.
