# Horumarka P2.9 — Ilaalinta bogga user-code

## Waxa la dhammaystiray

P2.9 wuxuu ku daray tijaabo toos ah oo ka socota ring-3: FSH wuxuu isku dayaa inuu wax ka beddelo byte ka mid ah bogga code-kiisa ee read-only. Tijaabadu waxay xaqiijisaa xadka u dhexeeya user mode iyo kernel-ka, mana aha kaliya hubin flags-ka page table-ka.

Probe-ka waxaa lagu xidhey slot go'an oo ku jira hal bogga FSH. Heshiiska offsets-ka waxaa wadaaga `kernel/fsh_probe_abi.h`; linker-ku wuxuu hubiyaa in code/rodata aysan slot-ka dul saarnayn, iyo in FSH.BIN uu weli ku jiro xadka hal bog. Probe-ku wuxuu ordaa oo keliya marka process-ku yahay PID 1.

Kernel-ku wuxuu soo celiyaa oo keliya page fault-ka saxda ah: CR2 wuxuu tilmaamayaa byte-ga probe-ka, error code-ku waa `0x7` (present/protection + write + user), EIP-gu waa tilmaanta store-ka, CS-guna waa `0x1b`. Waxa kale oo la xaqiijiyaa in PID-gu yahay kii la oggolaaday iyo in mapping-ka code-ku yahay user/read-only. Ka hor recovery-ga, byte-ga la qorayay waa inuu weli la mid yahay qiimihii bilowga ahaa (`0xE8`). Markaas oo keliya ayaa EIP loo leexiyaa resume label-ka FSH. Fault kasta oo kale wuxuu ku dhacaa jidkii hore ee fail-closed.

Waxaa sidoo kale la saxay diagnostics-ka #PF: ESP-ga user-ka waxaa la akhriyaa oo keliya marka frame-ku caddeeyo in fault-ku ka yimid ring-3; ring-0 frame-ka ma laha labada eray ee user ESP/SS.

## Xaqiijin

- `make -C kernel check` — wuu gudbay: BIOS build, QEMU smoke test, xadka loader-ka 120 sectors, diidmada 121 sectors, memory window-ka 16/32/64 MiB, iyo P2.7/P2.9 write-protection checks.
- `scripts/validate-repo.sh --uefi` — wuu gudbay: xaqiijinta repository-ga iyo build-ka UEFI.
- Serial trace-ku wuxuu xaqiijiyey in gate-ku diido near-matches-ka, in ring-3 write-ku xannibmay iyadoo byte-gu aanu isbeddelin, kadibna `fsh: disk executable online` uu muuqday. Taasi waxay caddeynaysaa in FSH uu ka soo laabtay recovery-ga oo uu sii watay bilowgiisa.
- Kernel binary-ga: 48,953 / 61,440 bytes; headroom: 12,487 bytes.
- FSH.BIN: 3,873 / 4,096 bytes; headroom: 223 bytes.

P2.6 iyo Phase 1 UTM ZIP-yadii hore lama beddelin. P2.9 waa source/runtime milestone; ZIP cusub looma samayn.

## Xuduudaha hadda jira

Tani waa tijaabo gaar ah oo BIOS 32-bit non-PAE ah, mana hirgelin NX ama ilaalin guud oo memory-ga user-ka oo dhan ah. Waxay sii xoojinaysaa keliya hal probe la yaqaan. UEFI Fadal64 iyo BIOS kernel-ku weli waa laba boot path oo kala duwan. Taageerada browser-ka, TLS/HTTPS, maareynta akoonnada badan, kaydinta sirta ah, iyo Unicode/RTL wali waxay u baahan yihiin milestones u gaar ah.

## Waxa xiga

Qorshaha xiga waa in si gooni-gooni ah loo caddeeyo jidka internet-ka ee UEFI (DNS/hostname iyo tijaabo live oo xaddidan), ka dibna la naqshadeeyo kaydinta aqoonsiga/password-ka ka hor UI-ga account setup. Qorshaha guud ee luqadaha, akoonnada shakhsiga ah, desktop-ka iyo UTM touch-ka wuxuu ku jiraa [roadmap-ka FadalWatch ee Af-Soomaaliga](ROADMAP_FADALWATCH_SO.md).
