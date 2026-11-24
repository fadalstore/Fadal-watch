# Horumarka P2.10 — UEFI networking

**Xaalad:** capability audit-ka, DHCP station, iyo HTTP GET-ga fixture-ka maxalliga ah waa la xaqiijiyey gudaha QEMU/OVMF. UTM candidate ZIP cusub waa la sameeyey oo la tijaabiyey. DNS/hostname weli lama xaqiijin; Internet guud iyo HTTPS/TLS lama tijaabin.

## Waxa la xaqiijiyey

UEFI loader-ku wuxuu tirinayaa handles-ka EFI Simple Network Protocol (SNP), wuxuu baaraa EFI_IP4_CONFIG2, HTTP Service Binding iyo EFI_HTTP, wuxuuna xogta capability-ga u gudbiyaa Fadal64. System panel-ka iyo amarka `net` waxay soo bandhigaan xaaladahaas. OVMF-ga la tijaabiyey wuxuu lahaa 3 SNP handles. `LocateProtocol(EFI_HTTP)` wuxuu soo celiyey `Not Found`, balse HTTP Service Binding wuu jiray; `CreateChild` iyo `OpenProtocol` labaduba waxay ku guulaysteen `Success`, taas oo siisay EFI_HTTP child la adeegsan karo. Tani waa helitaanka adeegga firmware-ka, mana aha kaligeed caddayn HTTP request shaqaynaya.

`EFI_IP4_CONFIG2` DHCP setup-ku wuxuu helay station `10.0.2.15/24` iyo mask `255.255.255.0`. Taasi waxay xaqiijinaysaa in guest-ku DHCP address helay; ma caddaynayso DNS ama Internet.

`make -C kernel/uefi test-network-audit` wuu gudbay. Tijaabadu waxay adeegsataa QEMU `e1000`, `-netdev user,...,restrict=on` iyo `guestfwd` ku xaddidan server fixture maxalli ah. Shell-ku wuxuu sameeyey plaintext `GET /fixture.txt` oo ku socda numeric IP `10.0.2.100`; fixture-ku wuxuu diiwaangeliyey request-ka, soo celiyey `200 OK`, waxaana `/home/root` lagu kaydiyey oo `cat` lagu akhriyey `FADAL_UEFI_HTTP_FIXTURE_OK`. Tani waxay caddaynaysaa oo keliya HTTP GET-ga fixture-kan xaddidan, ma aha browsing ama Internet guud.

`python3 -m py_compile scripts/test-uefi-network-audit.py scripts/uefi-http-fixture.py`, `git diff --check`, iyo `scripts/validate-repo.sh --uefi` way gudbeen. Validator-ku wuxuu xaqiijiyey BIOS freestanding/smoke tests iyo UEFI build-ka; boot-ka network-audit-ka iyo kormeerka System framebuffer-kana si gooni ah ayaa loo fuliyey.

Qabashada System panel-ka 1280×800 waa [network-audit-system.png](../kernel/uefi/out/network-audit-system.png). Muuqaal ahaan waa la kormeeray; wuxuu muujinayaa UEFI HTTP available, SNP exposed, HTTP Service Binding available, iyo in native Fadal64 DNS client aan la hirgelin.

## DNS/hostname: lama xaqiijin

Hostname-ka si gooni ah ayaa loo tijaabiyey iyadoo isla fixture-ka loo marayo URL `http://10.0.2.100.nip.io/fixture.txt`, halkii numeric IP si toos ah loo adeegsan lahaa. Natiijada shell-ku waxay ahayd `download: failed`, fixture-kuna ma helin request cusub (`new fixture requests=0`). Natiijadani ma kala saarayso DNS lookup fashilmay iyo isku-xir TCP/HTTP fashilmay. Sidaas darteed DNS iyo hostname resolution waa **lama xaqiijin**, mana sheeganayno inay shaqeeyaan ama gebi ahaanba maqan yihiin. Fadal64 wali ma laha native DNS client ama fallback.

Probe-ka hostname-ka waxaa lagu kicin karaa si gaar ah ka dib marka audit image la dhiso:

```sh
python3 scripts/test-uefi-network-audit.py \
  --image kernel/uefi/out/network-audit.img \
  --hostname-url http://10.0.2.100.nip.io/fixture.txt \
  --screenshot kernel/uefi/out/network-audit-dns-probe.ppm
```

Guuldarrada probe-kan kaligeed ma caddaynayso DNS failure; haddii dib loo tijaabiyo waa in fixture-ku helo request isla markaana marker-ka la soo celiyo ka hor inta aan hostname lookup loo aqoonsan mid la xaqiijiyey.

## UTM candidate package P2.10

[UTM ZIP-ga P2.10](../FadalOS-Phase2.10-UTM-iOS.zip) waa 161,481 bytes, SHA-256-giisuna waa `87ec98d8163da8ae0653fb7f615473452ad4b152a1617d21174bb94482b10cc9`. Sidecar checksum-ku waa `FadalOS-Phase2.10-UTM-iOS.zip.sha256`. Hashes-ka labada disk image ee gudaha waa:

- UEFI ESP: `c99208003db03f4aa43b0718d3b7cfbc11893425fac054a1310e72dd9e1c3127`
- BIOS disk: `f4a8ee750ec4868e51f4423ebed6987544d04e433921be1d35d1de304c4635d7`

Package builder-ku wuxuu xaqiijiyey ZIP integrity, internal `SHA256SUMS`, iyo byte equality-ga images-ka la dhisay. `sha256sum -c FadalOS-Phase2.10-UTM-iOS.zip.sha256` wuu gudbay. BIOS disk-ka gudaha ZIP-ga wuxuu la mid yahay image-kii `make -C kernel check` ku boot-gareeyey. UEFI disk-ka ZIP-ga laga soo saaray ayaa OVMF hoostiisa boot-gareeyey, wuxuuna mar kale gudbay DHCP iyo local fixture HTTP GET. System panel-ka image-kaas waa [p2.10-package-network-system.png](../kernel/uefi/out/p2.10-package-network-system.png).

Internet guud lama tijaabin. HTTPS/TLS, certificate validation, redirects, iyo browser features kale lama hirgelin ama lama tijaabin. `virtio-net-pci` laguma darin caddaynta; natiijada hadda jirta waxay khusaysaa QEMU `e1000` oo keliya. Helitaanka SNP, HTTP Service Binding, EFI_HTTP child, ama DHCP station midkoodna kaligiis ma caddaynayo awoodahaas.

Archives-ka Phase 1, P2.5, P2.6 iyo P2.8 lama beddelin; hashes-koodii hore ayaa la ilaaliyey. Package cusub oo P2.10 ah ayaa loo sameeyey filename gaar ah.
