# FadalOS Phase 2 — Work Plan

**Status:** P2.1–P2.10 complete for their scoped milestones; P2.11 configure-once reuse and two sequential numeric-IP fixture GETs are verified. DNS4 firmware-instance setup and hostname resolution remain unresolved; general Internet and HTTPS remain unverified.
**Baseline:** Phase 1 UTM release at commit `1d50d1d`
**Purpose:** define a staged, boot-testable path from the Phase 1 freestanding kernel and UEFI desktop prototype toward a more reliable, inspectable FadalOS runtime.

## Phase 2 principles

FadalOS remains a freestanding operating system. Linux, Ubuntu, GCC, QEMU, GNU-EFI, and similar host tools are build/test infrastructure only; no host kernel API, libc, shell, or userspace component may become a guest runtime dependency. The BIOS kernel and UEFI x86-64 payload remain separate boot paths unless a later architecture decision explicitly unifies them. Existing BIOS boot traces, syscall numbers, FDL64 headers, and UTM v4 profiles are compatibility contracts.

Each milestone must be small enough to build and verify independently. Do not claim a graphical or boot feature complete from compilation alone: boot it under QEMU/OVMF (and inspect the framebuffer when relevant), run repository validation, document known limits, and retain a recoverable commit.

## Milestones

### P2.1 — Read-only shell history command *(complete)*

Add a `history` command to the UEFI shell that displays the retained command ring in oldest-to-newest order. The existing eight-entry bound remains authoritative; printing the list adds no extra allocation and does not modify retained entries beyond the normal recording of the submitted `history` command itself. Empty Enter remains excluded, and the `history` command appears as the newest entry. `help` lists the command; entries remain guest-local and volatile.

**Verified:** `make -C kernel/uefi test-terminal-history` checks empty submissions, navigation/draft restoration, recalled-command editing, wraparound, exact oldest-to-newest output order, and the eight-entry display bound. `scripts/validate-repo.sh --uefi` passed, and the final QEMU/OVMF framebuffer was inspected at `kernel/uefi/out/terminal-history.ppm`.

### P2.2 — Regression-suite reliability *(complete)*

Investigate two known test issues without relaxing assertions:

- `test-window-controls` has failed on different Files navigation checks across reruns (Backspace from Downloads and Up returning to the first page). Determine whether the cause is QEMU input timing, focus routing, or a guest defect; add deterministic evidence and rerun the full target.
- `test-persistent-home` passed its guest serial checks through reboot, but the Make target's final host-side `mcopy` check did not find `PERSIST` in its source image. Trace which image the guest modified and which image the host inspects; make both inspect the same artifact and preserve the raw-content assertions.

**Resolution and verification:** `test-window-controls` now polls for both the Files titlebar and the exact expected selected-row highlight, so an incomplete redraw cannot satisfy navigation. A bounded three-attempt HMP delivery loop recovers occasional lost QEMU arrow-key injections and reports any retry; it does not skip or alter the row, scroll, Enter, Backspace, Home, or End assertions. The full target passed three consecutive post-fix QEMU/OVMF boots; one boot recorded two recovered `Down` deliveries. `test-persistent-home` now runs QEMU against the exact image that Make later inspects with `mcopy` rather than a temporary copy. The guest reboot checks and host-side byte-equality, deleted-file absence, nested deletion, and retained-directory checks all passed against `kernel/uefi/out/persistent-home.img`.

### P2.3 — BIOS runtime architecture decision *(complete)*

Before choosing a large kernel feature, write a short architecture decision record comparing the two supported directions: continue expanding the 32-bit BIOS runtime (whose identity-mapped memory range is currently bounded) or introduce a separate x86-64 BIOS boot path. Include memory-map/allocator limits, interrupt and syscall contracts, process-context switching, SMP/AP startup prerequisites, boot compatibility, and the cost of maintaining two paths. Keep the existing 32-bit BIOS image bootable while evaluating options.

**Decision:** [ADR 0001](adr/0001-bios-runtime-architecture.md) keeps the existing 32-bit BIOS runtime and compatibility contracts, defers a separate x86-64 BIOS runtime, and authorizes only a 16-to-64 MiB non-PAE identity-map/allocator proof for P2.4. The ADR records the memory, interrupt/syscall, process, SMP, boot-compatibility, and maintenance tradeoffs plus exact QEMU acceptance checks. No BIOS syscall number or smoke trace changed as a side effect of the decision.

### P2.4 — First selected kernel-runtime milestone *(complete)*

P2.4 extends the existing 32-bit non-PAE identity map and E820-backed allocator window from 16 MiB to at most 64 MiB. The no-E820 fallback remains bounded at 16 MiB. User code/stack/heap page tables remain private; identical supervisor-only tables above the first 8 MiB are shared to reduce per-process table storage while preserving the full identity map. The BIOS image layout, syscall 1–34 ABI, and legacy smoke trace are unchanged.

**Verified:** `make -C kernel check` passes the original serial smoke contract and runs `test-memory-window` under QEMU TCG at 16 MiB (negative control: no high-page allocation; allocator remains E820-bounded), 32 MiB, and 64 MiB. The 32/64 MiB runs allocate a usable E820 page above 16 MiB, write/read a full-page pattern, free/reuse the same page, and verify low-reserved, E820-unusable when present, and out-of-window bounds. The complete BIOS check also confirms process address-space isolation, private user-page mapping, ring-3 syscall dispatch, and timer context switching remain intact.

### P2.5 — Phase 2 release checkpoint *(complete)*

**Verified:** `scripts/validate-repo.sh --uefi` and the complete UEFI QEMU/OVMF regression matrix passed. The separate `FadalOS-Phase2-UTM-iOS.zip` contains the current UEFI image and the P2.4 BIOS image in distinct UTM profiles. Each staged profile booted under its documented QEMU/OVMF configuration; ZIP integrity and per-image/archive SHA-256 checks passed. The Phase 1 UTM ZIP aliases and their manifest/installer inputs were not changed. See [Phase 2 release record](PHASE2_RELEASE.md) for checksums, test evidence, and known limits.

**P2.5 artifact constraint at release:** the BIOS payload in `FadalOS-Phase2-UTM-iOS.zip` uses 57,273 of its fixed 57,344-byte (112-sector) loader budget, leaving 71 bytes. That archive and both Phase 1 archives remain historical and unchanged. The P2.6 source build below provides a larger, separately tested loader window; no existing ZIP/profile is silently replaced.

### P2.6 — Bounded BIOS loader capacity *(complete)*

Raise the current BIOS build's reserved kernel window from 112 to 120 sectors (61,440 bytes) while retaining the 512-byte boot sector, legacy INT 13h CHS path, `0x10000` load address, and i386 protected-mode entry. Embed the actual rounded-up kernel-sector count; halt before disk I/O if it is zero or exceeds the explicit 120-sector ceiling. Derive the FAT12 volume offset from this same ceiling, moving it to LBA 121 for newly built images.

**Verified:** `make -C kernel check` passes the unchanged BIOS serial contract, an exact-61,440-byte size acceptance and 61,441-byte rejection, a QEMU positive control that boots with the maximum 120-sector load count, a QEMU negative control that changes the embedded count to 121 and requires the bootloader rejection marker with no kernel-entry trace, and the existing 16/32/64 MiB memory-window matrix. `scripts/validate-repo.sh --uefi` and the full UEFI QEMU/OVMF matrix also pass. The new `FadalOS-Phase2.6-UTM-iOS.zip` boots from both extracted profiles; its UEFI and BIOS disk images are checksum-verified. The Phase 1 packages and P2.5 Phase 2 ZIP remain byte-for-byte unchanged. See [ADR 0001](adr/0001-bios-runtime-architecture.md), [build instructions](BUILD.md), and [release record](PHASE2_RELEASE.md) for the new raw-disk layout and hashes.

### P2.7 — BIOS supervisor write protection *(complete; source checkpoint only)*

The BIOS linker now defines a page-aligned read-only `.text`/`.rodata` range and a separate writable `.data`/BSS range. Paging removes `PAGE_WRITE` from the kernel read-only pages; after paging, GDT, and IDT initialization, the kernel sets `CR0.WP` so supervisor writes obey those PTEs. A dedicated `.rodata` byte is used for a controlled ring-0 store probe. The page-fault handler recovers only when CR2, the write/protection error bits, saved EIP, and privilege level match that exact probe; every other fault retains the fail-closed path. Runtime checks verify writable supervisor mappings for `.data`, BSS-backed page tables/page directories, and the IDT, and verify that both process address spaces preserve the kernel's read-only mappings. Syscall copyout paths reject the read-only user-code page; FSH's mutable command, file, and authentication state now lives on its writable user stack. This is supervisor write protection, not NX support: the 32-bit non-PAE BIOS path still cannot mark pages non-executable.

**Verified:** `make -C kernel check` passes the existing BIOS smoke/ABI contract, 16/32/64 MiB memory matrix, exact/over-size guards, maximum 120-sector load and 121-sector rejection controls, the CR0.WP expected-fault recovery, and both process-space mapping checks. `scripts/validate-repo.sh --uefi` passes. All eight UEFI QEMU/OVMF regression targets (page protection, focus, terminal editing, selection, history, window controls, resize, and persistent-home) passed across the complete serial run and targeted reruns. One aggregate run exposed transient HMP/focus timing failures in history and the persistent-home startup focus check; each affected target passed when rerun alone. The BIOS binary is 61,305/61,440 bytes, leaving only 135 bytes. No UTM archive was regenerated: P2.6 and Phase 1 packages remain frozen and do not contain this P2.7 source change.

### P2.8 — BIOS image headroom by size optimization *(complete; source and package verified)*

P2.7 left only 135 bytes in the fixed 120-sector BIOS kernel window. Before adding more kernel code, the default freestanding BIOS C optimization was changed from `-O2` to `-Os`; the loader ceiling, 120-sector guard, raw-disk layout, BIOS ABI, and guest runtime dependencies were not changed.

**Natiijo la xaqiijiyey:** kernel binary-gu wuxuu ka yaraaday 61,305 bytes ilaa 47,609 bytes, taas oo badbaadisay 13,696 bytes kana tagtay 13,831 bytes oo headroom ah gudaha xadka 61,440-byte. Boot sector-ku wuxuu ku dhex-daray tirada dhabta ah ee 93 sectors; xadka loader-ku weli waa 120 sectors, FAT12-na weli wuxuu ka bilaabmaa LBA 121. `make -C kernel check` iyo `scripts/validate-repo.sh --uefi` labaduba way gudbeen. Waxaa la sameeyey `FadalOS-Phase2.8-UTM-iOS.zip` oo gaar ah; checksum-ka, hashes-ka gudaha, QEMU BIOS boot-ka, UEFI OVMF window-controls/focus, iyo EFI page-protection runtime test way gudbeen. P2.6 iyo Phase 1 archives waa la ilaaliyey oo lama beddelin. Faahfaahinta hashes-ka iyo tijaabooyinka waxay ku jiraan [release record](PHASE2_RELEASE.md) iyo [warbixinta Somali-ga](HORUMAR_P2_8.md).

### P2.9 — Tijaabada ring-3 ee code-page read-only *(complete; source/runtime checkpoint)*

FSH wuxuu sameeyaa store toos ah isagoo ku jira ring-3, isaga oo isku dayaya inuu wax ka beddelo byte ku jira boggiisa code-ka ee read-only. Probe-ku wuxuu ku jiraa slot go'an oo linker-ku ilaaliyo gudaha xadka hal bog ee FSH.BIN; wuxuu ordaa oo keliya PID 1. Handler-ku wuxuu aqbalaa oo keliya CR2, error code `0x7`, faulting EIP, CS `0x1b`, PID iyo mapping-ka read-only ee la filayo. Ka hor inta uusan EIP u leexin resume label-ka FSH, wuxuu xaqiijiyaa in byte-gii code-ku aanu isbeddelin. Fault kasta oo kale wuxuu sii raacayaa fail-closed path-ka. Diagnostics-ku ESP-ga user-ka waxay akhriyaan oo keliya marka saved CS caddeeyo privilege ring-3.

**La xaqiijiyey:** `make -C kernel check` wuxuu gudbay BIOS/QEMU boot smoke, loader ceiling/121-sector rejection, 16/32/64 MiB memory matrix iyo P2.7/P2.9 write-protection checks. `scripts/validate-repo.sh --uefi` sidoo kale wuu gudbay. Serial output-ku wuxuu xaqiijiyey near-match gate-ka, byte aan isbeddelin iyo in FSH dib ugu laabtay bilowgiisa. BIOS kernel-ku waa 48,953/61,440 bytes (12,487 bytes oo headroom ah); FSH.BIN waa 3,873/4,096 bytes (223 bytes oo headroom ah). P2.9 waa source/runtime checkpoint; ZIP cusub lama samayn, P2.6 iyo Phase 1 artifacts-na lama beddelin. Warbixinta Somali-ga: [HORUMAR_P2_9](HORUMAR_P2_9.md).

### P2.10 — UEFI network capability audit *(scoped DHCP/HTTP fixture verified; DNS unresolved)*

UEFI loader-ku wuxuu tirinayaa handles-ka EFI Simple Network Protocol (SNP), wuxuuna ogaadaa HTTP Service Binding iyo EFI_HTTP protocol-ka. Awoodaha la helay waxa loo gudbiyaa Fadal64; System panel-ka iyo `net` command-ku waxay soo bandhigaan xaalad isku mid ah. Hostname lookup waxa firmware-ka u qaban lahaa EFI_HTTP marka la helo; native Fadal64 DNS client/fallback lama hirgelin.

**La xaqiijiyey:** `make -C kernel/uefi test-network-audit` wuxuu boot-gareeyey QEMU 8.2.2/OVMF 2024.02-2ubuntu0.10 isagoo wata e1000 iyo user networking xaddidan. Loader-ku wuxuu arkay 3 SNP handles; `LocateProtocol(EFI_HTTP)` wuxuu ahaa `Not Found`, balse HTTP Service Binding, `CreateChild`, iyo `OpenProtocol` way guulaysteen. DHCP wuxuu helay `10.0.2.15/24`. Plaintext GET ku socda numeric IP `10.0.2.100` wuxuu gaaray `guestfwd` fixture-ka, wuxuuna soo celiyey marker-ka `FADAL_UEFI_HTTP_FIXTURE_OK`. `net` command-ku wuxuu la jaanqaaday xogta loader-ka, framebuffer-ka System panel-ka waa la kormeeray, package-ka P2.10-na waa la sameeyey oo la boot-tijaabiyey. `scripts/validate-repo.sh --uefi` wuu gudbay.

**Xaddidaad:** hostname `10.0.2.100.nip.io` probe-gii hore wuu fashilmay markii mar labaad la wacay EFI_HTTP `Configure` (`Already started`, `stage=4`); taasi ma ahayn caddayn DNS failure. P2.11 wuxuu hagaajiyey lifecycle-ka Configure-once, waxaana la xaqiijiyey laba numeric-IP GET oo xiriir ah oo fixture-ka gaaray kuna soo celiyey marker-kooda. Markii hostname probe-ga dib loo tijaabiyey, `EFI_HTTP.Request` wuxuu soo celiyey `Not Found` (`stage=5`, `HTTP=0`), fixture-kuna ma arag request hostname cusub. Pcap-ku wuxuu lahaa eber port 53 ah iyo eber TCP session cusub oo hostname-ka ah, sidaas darteed sababtu wali lagama kala saarin DNS lookup iyo diidmada/maqnaanshaha transport-ka. DNS4 Service Binding waa la helay, laakiin `EFI_DNS4` protocol instance lama helin; child-instance creation iyo `HostNameToIp` wali lama tijaabin. Numeric-IP GET-gu wuxuu ku koobnaa fixture maxalli ah; Internet guud lama tijaabin. HTTPS/TLS lama hirgelin ama lama tijaabin. Faahfaahinta: [HORUMAR P2.10](HORUMAR_P2_10.md), [HORUMAR P2.11](HORUMAR_P2_11.md), iyo [release record](PHASE2_RELEASE.md).

### P2.11 — Dib-u-isticmaalka EFI_HTTP iyo hostname diagnosis *(dib-u-isticmaalka waa la xaqiijiyey; hostname diagnosis wali socda)*

EFI_HTTP hal mar oo keliya configure-garee, kadibna u isticmaal dhowr Request/Response oo isku xiga; ha ugu yeerin Configure mar kale iyadoo session-ku bilowday. Regression-ka laba numeric-IP GET ah waa la hirgeliyey, labaduna fixture-ka way gaareen oo marker-ka way soo celiyeen. Hostname probe-ga dib loo tijaabiyey wuxuu ku dhacay EFI_HTTP `Request` oo soo celiyey `Not Found` (`stage=5`, `HTTP=0`); fixture-ku ma arag request hostname cusub. Pcap-ku ma lahayn DNS port 53 traffic ama TCP connection hostname cusub, sidaas darteed sababta wali lama kala saarin. Loader-ku wuxuu helay DNS4 Service Binding laakiin ma helin `EFI_DNS4` protocol instance. Tallaabada xigta waa in la tijaabiyo abuurista DNS4 child instance, `Configure`, iyo `HostNameToIp` ka hor go'aan native resolver.

**Xaaladda aqbalaadda:** qaybta session reuse way gudubtay: labada sequential numeric-IP GET waxay gaareen fixture-ka, marker-koodana waa la akhriyey. Qaybta hostname diagnosis wali ma gudbin, maaddaama `EFI_HTTP.Request` `Not Found` ku dhammaaday, pcap-kuna aanu muujin DNS ama hostname TCP. Hostname resolution lama sheeganayo ilaa hostname fixture request iyo marker la arko. Internet guud iyo HTTPS/TLS weli waa xuduud ka baxsan P2.11.

### P2.12 — DNS4 child instance iyo HostNameToIp audit *(child/configure/query submission waa la xaqiijiyey; DNS transport wali lama xaqiijin)*

Loader-ku hadda wuxuu ka sameeyaa DNS4 Service Binding child protocol instance, wuxuu furaa `EFI_DNS4_PROTOCOL`, wuxuu ku sameeyaa `Configure` iyadoo adeegsanaysa DHCP/default settings, kadibna wuxuu diraa `HostNameToIp`. QEMU/OVMF wuxuu xaqiijiyey child creation, `OpenProtocol`, Configure, iyo query submission. Hostname HTTP probe-gu weli wuxuu ku dhammaaday `EFI_HTTP.Request` `Not Found`, fixture-kuna request ma helin; P2.12 pcap-kuna wuxuu lahaa eber port-53 packets. Sidaas darteed DNS lookup completion iyo DNS transport lama xaqiijin, mana jiro native resolver ama HTTPS/TLS wali. Faahfaahinta: [HORUMAR P2.12](HORUMAR_P2_12.md).

## Immediate work queue

1. P2.1 is complete: the UEFI `history` output and exact-order QEMU regression are in place.
2. P2.2 is complete: the window-controls test is state-synchronized and retry-bounded, and persistent-home guest/host checks use the same disk image.
3. P2.3 is complete: ADR 0001 keeps the i386 BIOS runtime and defers a separate x86-64 BIOS path.
4. P2.4 is complete: 16/32/64 MiB QEMU controls validate the bounded non-PAE memory window.
5. P2.5 is complete: the Phase 2 artifact is separate, boot-verified, checksummed, and documented; Phase 1 archives remain frozen.
6. P2.6 is complete: the 120-sector load window and byte ceiling have exact-limit and QEMU 121-sector rejection controls.
7. P2.7 is complete: BIOS `.text`/`.rodata` are supervisor read-only under CR0.WP, an intentional write fault is precisely recovered, and process-space permissions are checked.
8. P2.8 is complete: the BIOS default uses `-Os`, the kernel is 47,609/61,440 bytes, and the separately versioned P2.8 UTM ZIP passed checksum and boot tests. The P2.6 and Phase 1 archives remain unchanged.
9. P2.9 waa complete: FSH ring-3 wuxuu tijaabiyaa store-to-code; kernel-ku wuxuu soo celiyaa oo keliya fault-ka si sax ah u waafaqa, wuxuuna diidaa near-matches. QEMU/validator tests way gudbeen; archives-kii hore waa la ilaaliyey.
10. P2.10 waa dhammaaday xadka la qeexay: DHCP iyo numeric-IP plaintext GET-ga fixture maxalliga ah waa la xaqiijiyey; hostname/DNS, Internet guud iyo HTTPS lama xaqiijin.
11. P2.11 session reuse-ku waa la xaqiijiyey: labada numeric-IP GET ee xiriirka ah waxay soo celiyeen marker-ka. DNS4 Service Binding waa la helay, balse protocol instance lama helin; hostname probe-ga `EFI_HTTP.Request` ayuu `Not Found` ku fashilmay, pcap-kuna DNS/hostname TCP ma muujin. Xiga waa tijaabada DNS4 child-instance/`HostNameToIp`; hostname, Internet guud iyo HTTPS lama sheeganayo. Faahfaahinta ku jirta [HORUMAR P2.11](HORUMAR_P2_11.md). Himilada luqadaha, akoonnada shakhsiga ah iyo desktop-ka UTM waxaa lagu diiwaangeliyey [roadmap-ka Somali-ga](ROADMAP_FADALWATCH_SO.md).

## Out of scope for this plan

This plan does not promise a complete userspace desktop, HTTPS/TLS, multi-user security, persistent command history, a unified BIOS/UEFI kernel, or SMP support. Those require separate designs and testable contracts.

### P2.13 — DNS4 firmware behavior diagnosis *(Configure modes la kala saaray; transport wali lama xaqiijin)*

Tijaabada P2.13 waxay kala saartay laba firmware behavior: `UseDefaultSetting=TRUE` wuxuu OVMF ku soo celiyey `EFI_DNS4 Configure = No mapping`; explicit DHCP station/subnet mapping iyo QEMU slirp DNS `10.0.2.3` waxay soo celiyeen `Configure = Success`, laakiin `HostNameToIp` ayaa ku xannibmay ka hor submit-status diagnostic, iyadoo pcap-ku aanu muujin port-53 traffic. Explicit-DNS experiment-ka lama gelin source-ka sababtoo ah wuxuu regression ku keenay boot audit-ka. P2.12 source iyo ZIP-ga waa baseline-ka la ilaaliyey. Faahfaahinta: [HORUMAR P2.13](HORUMAR_P2_13.md). Tallaabada xigta waa design/test native bounded DNS fallback ama asynchronous firmware adapter; hostname/DNS lama sheeganayo inuu shaqeeyo.

## P2.13 immediate next step

Native DNS fallback ama firmware adapter waa in lagu sameeyaa design/test gaar ah oo leh timeout, packet-level evidence iyo failure-safe boot behavior; `EFI_DNS4 HostNameToIp` lama wici doono si aan xad lahayn inta laga helayo contract sax ah.
