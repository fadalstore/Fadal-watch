# Horumarka P2.11 — Dib-u-isticmaalka EFI_HTTP iyo DNS4 audit

**Xaalad:** hagaajinta Configure-once iyo regression-ka laba GET oo xiriir ah waa la tijaabiyey oo way gudbeen. Baaritaanka hostname-ku weli ma kala saarin DNS lookup iyo diidmada `EFI_HTTP.Request`; sidaas darteed P2.11 oo dhan wali lama xirin. DNS/hostname, Internet guud iyo HTTPS/TLS lama xaqiijin.

## Natiijooyinka QEMU/OVMF

`make -C kernel/uefi test-network-audit` wuu gudbay. Tijaabadu waxay ku boot-gareysay QEMU/OVMF oo adeegsanaya `e1000`, `restrict=on`, iyo HTTP fixture maxalli ah. OVMF wuxuu soo bandhigay 3 SNP handles. `EFI_HTTP` laguma helin `LocateProtocol`, laakiin HTTP Service Binding, `CreateChild`, iyo `OpenProtocol` way guulaysteen. DHCP wuxuu helay `10.0.2.15/24`.

Hagaajinta `g_fadal_http_configured` waxay ka hortagtaa in isla `EFI_HTTP` child-ka mar labaad la `Configure` gareeyo. Laba numeric-IP plaintext GET oo isku xiga ayaa labaduba gaaray `10.0.2.100` fixture-ka, waxaana mid kasta laga akhriyey marker-ka `FADAL_UEFI_HTTP_FIXTURE_OK`. Tani waxay xaqiijinaysaa dib-u-isticmaalka session-ka labadan GET ee xaddidan oo keliya.

Loader audit-ku wuxuu helay **EFI DNS4 Service Binding: available**, laakiin `LocateProtocol(EFI_DNS4)` wuxuu ahaa **unavailable**. GUID-yada la hubiyey waa `EFI_DNS4_PROTOCOL` `{ae3d28cc-e05b-4fa1-a011-7eb55a3f1401}` iyo Service Binding `{b625b186-e063-44f7-8905-6a74dc6f52b4}`.[2] UEFI 2.10 wuxuu qeexayaa DNSv4 Service Binding iyo `EFI_DNS4_PROTOCOL`, oo ay ku jirto habka `HostNameToIp`; helitaanka binding-ka kaligiis macnaheedu ma aha in DNS client instance la abuuray ama hostname resolution shaqaynayo. [1] Natiijada QEMU/OVMF waa capability audit, ma aha tijaabo DNS lookup.

System panel-ka waa la boot-gareeyey oo muuqaal ahaan waa la kormeeray. Wuxuu si sax ah u muujinayaa DNS4 Service Binding available, DNS4 protocol instance unavailable, iyo in native Fadal64 DNS client aan la hirgelin: [sawirka System panel-ka](../kernel/uefi/out/network-audit-p2.11-system.png).

## Hostname probe iyo packet capture

Probe-ga `http://10.0.2.100.nip.io/fixture.txt` wuxuu ku dhammaaday `download: failed`; fixture-ku ma diiwaangelin request hostname cusub. EFI_HTTP diagnostics-ku waxay muujiyeen `stage=5 status=Not Found HTTP=0`, taas oo ah fashilka marka `EFI_HTTP.Request` la waco, ka dib Configure-once. Sidaas darteed guuldarradu ma aha caddayn gooni u ah DNS failure ama connection failure.

QEMU `filter-dump` pcap-ku wuxuu qabtay 30 frame. Parser-ka Ethernet/IPv4/UDP/TCP wuxuu helay 6 UDP frame oo dhammaantood ahaa DHCP, 19 IPv4/TCP frame oo ka tirsan laba xiriir oo numeric-IP fixture GET ah (`10.0.2.15` port-yada 1830/1831 iyo `10.0.2.100:80`), iyo **eber** UDP/TCP packet oo port 53 ah. Ma jirin TCP session cusub oo hostname-ku keenay. Sababtoo ah hostname request-ku wuxuu ku fashilmay EFI_HTTP `Request` oo pcap-kuna kuma jiraan DNS packet, xogtani waxay tilmaamaysaa oo keliya inaan DNS ama hostname transport traffic ka muuqan capture-kan; ma caddeyneyso sababta gudaha firmware-ka. QEMU user networking-ku `restrict=on` ayuu ahaa, sidaas darteed ma jirin tijaabo Internet guud.

[Pcap-ka P2.11](../kernel/uefi/out/network-audit-p2.11.pcap) ayaa ah caddaynta cayriin ee capture-ka. `EFI_RNG` sidoo kale wuxuu ku soo baxay unavailable OVMF-kan; xogtaasi waxay xusuusin u tahay naqshadda TLS mustaqbalka, mana beddelayso natiijada HTTP fixture-ka.

## Tallaabada xigta

Ka hor sheegashada DNS ama bilaabidda resolver Fadal64, tallaabada xigta waa in si gooni ah loo tijaabiyo DNS4 Service Binding-ka firmware-ka: aqoonsi network controller-ka ku habboon, ka samee child protocol instance adigoo adeegsanaya Service Binding-ka, configure-garee DNS4, dabadeed `HostNameToIp` ku samee tijaabo maxalli ah oo la cabbiri karo. Haddii habkaas ama DNS packet-ku aanu shaqayn, markaas go'aan naqshadeed ayaa qeexaya in la hagaajiyo firmware path-ka ama la hirgeliyo native DNS resolver. Ilaa hostname request-ka fixture-ku gaaro oo marker-kiisa soo celiyo, DNS/hostname waa **aan la xaqiijin**. Internet guud iyo HTTPS/TLS weli waa aan la xaqiijin.

## Tijaabooyinka la orday

- `python3 -m py_compile scripts/test-uefi-network-audit.py`
- `git diff --check`
- `make -C kernel/uefi test-network-audit`
- `python3 scripts/test-uefi-network-audit.py --image kernel/uefi/out/network-audit.img --hostname-url http://10.0.2.100.nip.io/fixture.txt --pcap kernel/uefi/out/network-audit-p2.11.pcap --screenshot kernel/uefi/out/network-audit-p2.11-system.ppm`

## References

[1]: https://uefi.org/sites/default/files/resources/UEFI_Spec_2_10_Aug29.pdf "UEFI Specification 2.10, section 29.4 EFI DNSv4 Protocol"
[2]: https://raw.githubusercontent.com/tianocore/edk2/master/MdePkg/Include/Protocol/Dns4.h "TianoCore EDK II DNS4 protocol header"
