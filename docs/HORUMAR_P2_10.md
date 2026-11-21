# Horumarka P2.10 — Audit-ka UEFI networking

**Xaalad:** qaybta capability-audit waa la hirgeliyey oo la tijaabiyey; P2.10 weli wuu socdaa sababtoo ah live HTTP fixture lama gaadhin.

## Waxa la hirgeliyey

UEFI loader-ku hadda wuxuu tirinayaa handles-ka leh EFI Simple Network Protocol (SNP), wuxuu baaraa HTTP Service Binding iyo EFI_HTTP protocol-ka, wuxuuna awoodaha la helay u gudbiyaa Fadal64. **System** panel-ka iyo amarka `net` waxay muujiyaan isla xaaladaha: SNP, HTTP Service Binding, EFI_HTTP, iyo meesha hostname lookup-ku ka dhacayo.

Hostname resolution-ka UEFI HTTP request-ka waxaa qaban lahaa firmware-ka iyada oo loo marayo EFI_HTTP marka adeeggaas la helo. Fadal64 ma laha native DNS client ama fallback. Sidaa darteed fariintaasi waxay sharxaysaa xadka naqshadda; ma caddaynayso in DNS lookup uu guulaystay. Tijaabadani ma dirin DNS query.

## Caddaynta tijaabada

- `make -C kernel/uefi test-network-audit` wuu gudbay. Wuxuu boot-gareeyey image ku-meelgaar ah gudaha QEMU/OVMF, `-netdev user,id=net0,restrict=on` iyo `e1000`; wuxuu isbarbardhigay loader diagnostics-ka iyo wax-soo-saarka `net`, dabadeedna wuxuu kaydiyey System panel-ka.
- OVMF/QEMU-ga la tijaabiyey wuxuu soo bandhigay 3 SNP protocol handles iyo HTTP Service Binding, laakiin EFI_HTTP protocol-ku ma furnayn. `download`/browser HTTP markaas lama adeegsan karo.
- Screenshot-ka 1280×800 ee [System panel-ka](../kernel/uefi/out/network-audit-system.png) waa la kormeeray; wuxuu si cad u muujinayaa `UEFI HTTP: UNAVAILABLE`, `HTTP SERVICE BINDING: AVAILABLE`, iyo in DNS client-ka Fadal64 aan la hirgelin.
- `scripts/validate-repo.sh --uefi` wuu gudbay, oo ay ku jiraan BIOS smoke/independence checks iyo build-ka UEFI.

## Waxa aan la xaqiijin

Ma jiro HTTP GET live ah, DNS query, Internet access, ama HTTPS/TLS oo lagu xaqiijiyey. OVMF-ga la tijaabiyey ma bixin EFI_HTTP protocol-ka loo baahan yahay; sidaas darteed ma jirin qaab lagu fuliyo live fixture-ka iyada oo aan la beddelin firmware-ka. SNP handle ama HTTP Service Binding oo la arkay keligiis ma caddaynayo in request shaqaynayo.

## Tallaabada xigta

Hel ama dhis OVMF/UEFI image bixiya EFI_HTTP si dhab ah u shaqaynaya, kuna xidh fixture HTTP oo maxalli ah oo xaddidan. Marka hore xaqiiji GET plaintext HTTP iyo natiijo xadidan; DNS hostname lookup si gooni ah u xaqiiji, hana isku darin labada sheegasho. Haddii firmware shaqaynaya la waayo, qiimee naqshad native DNS/HTTP oo Fadal64 ah iyo tijaabooyinkeeda ka hor ballan-qaad internet. HTTPS waxa ay ahaanaysaa wax aan la hirgelin ilaa TLS, entropy iyo certificate validation la naqshadeeyo lana tijaabiyo.

UTM ZIP-yadii taariikhiga ahaa lama beddelin, archive cusubna lama samayn.