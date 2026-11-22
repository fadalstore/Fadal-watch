# Horumarka P2.10 — UEFI networking

**Xaalad:** capability audit-ka, DHCP station, iyo HTTP GET-ga fixture-ka maxalliga ah waa la xaqiijiyey gudaha QEMU/OVMF. DNS/hostname weli lama xaqiijin; Internet guud iyo HTTPS/TLS lama tijaabin.

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

## Waxa aan la xaqiijin

Internet guud lama tijaabin. HTTPS/TLS, certificate validation, redirects, iyo browser features kale lama hirgelin ama lama tijaabin. `virtio-net-pci` laguma darin caddaynta; natiijada hadda jirta waxay khusaysaa QEMU `e1000` oo keliya. Helitaanka SNP, HTTP Service Binding, EFI_HTTP child, ama DHCP station midkoodna kaligiis ma caddaynayo awoodahaas.

UTM ZIP-yada versioned-ka ah lama beddelin, archive cusubna lama samayn.
