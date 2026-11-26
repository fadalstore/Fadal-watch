# Horumarka P2.12 — DNS4 child instance iyo HostNameToIp audit

**Xaalad:** DNS4 Service Binding-ka firmware-ka hadda si gooni ah ayaa loo tijaabiyey. Loader-ku wuxuu sameeyaa `CreateChild`, wuxuu furaa `EFI_DNS4_PROTOCOL`, wuxuu ku sameeyaa `Configure` iyadoo adeegsanaysa DHCP DNS/default settings, kadibna wuxuu diraa `HostNameToIp` query. Tani waxay kala saartay maqnaanshaha protocol instance iyo fashilka DNS transport-ka.

## Waxa la xaqiijiyey

- OVMF wuxuu bixiyaa DNS4 Service Binding.
- DNS4 child protocol instance ayaa si guul leh loo abuuray.
- `OpenProtocol` ayaa helay child-ka `EFI_DNS4_PROTOCOL`.
- `EFI_DNS4.Configure` ayaa ku dhammaaday success iyadoo `DnsServerListCount=0` iyo `UseDefaultSetting=TRUE`, si firmware-ku uga soo qaato DHCP DNS configuration.
- `EFI_DNS4.HostNameToIp` query ayaa la submit-gareeyey.
- System panel-ka iyo `net` command-ku waxay hadda muujiyaan DNS4 protocol instance-ka final-ka ah, kaas oo noqon kara direct instance ama child instance.

## Waxa aan weli la xaqiijin

Hostname probe-ga `http://10.0.2.100.nip.io/fixture.txt` weli wuxuu ku dhammaaday `download: failed`, iyadoo `EFI_HTTP.Request` uu soo celiyey `stage=5 status=Not Found HTTP=0`. Fixture-ku ma helin request hostname cusub.

Pcap-ka P2.12 wuxuu qabtay 30 frame, laakiin ma lahayn wax UDP ama TCP packet ah oo port 53 ah. Sidaas darteed OVMF path-ku wuxuu aqbalay DNS4 child/configure/query submission, laakiin lookup completion iyo DNS transport-ka lama xaqiijin. Tani waxay weli ka tagaysaa laba suurtagal: OVMF ma helin DNS server configuration la adeegsan karo, ama query-ga ayaa gudaha firmware-ka ku fashilmay ka hor inta uusan packet dirin.

Internet guud, native Fadal64 DNS resolver iyo HTTPS/TLS weli lama hirgelin ama lama tijaabin.

## Tijaabooyinka

```sh
make -C kernel/uefi test-network-audit \
  CC=x86_64-linux-gnu-gcc \
  LD=x86_64-linux-gnu-ld \
  OBJCOPY=x86_64-linux-gnu-objcopy \
  OBJDUMP=x86_64-linux-gnu-objdump \
  NM=x86_64-linux-gnu-nm \
  EFI_INC=/path/to/gnu-efi/usr/include/efi \
  EFI_ARCH_INC=/path/to/gnu-efi/usr/include/efi/x86_64 \
  EFI_CRT=/path/to/gnu-efi/usr/lib/crt0-efi-x86_64.o \
  EFI_LDSCRIPT=/path/to/gnu-efi/usr/lib/elf_x86_64_efi.lds \
  EFI_LIBDIR=/path/to/gnu-efi/usr/lib
```

Hostname probe iyo pcap:

```sh
python3 scripts/test-uefi-network-audit.py \
  --image kernel/uefi/out/network-audit.img \
  --hostname-url http://10.0.2.100.nip.io/fixture.txt \
  --pcap kernel/uefi/out/network-audit-p2.12.pcap \
  --screenshot kernel/uefi/out/network-audit-p2.12-system.ppm
```

Natiijada QEMU/OVMF ee la helay: sequential numeric-IP GET-kii labaad wuu gudbay; DNS4 child/configure/query submission way gudbeen; hostname request iyo port-53 traffic ma aysan gudbin. ZIP-yadii P2.10 iyo kuwii ka horreeyey lama taaban.

## Tallaabada xigta

Tallaabada xigta waa in DHCP/IPv4 configuration-ka laga soo akhriyo DNS server list-ka firmware-ka, ama la sameeyo probe DNS4 leh server address si cad loo siiyo gudaha fixture environment-ka. Haddii taas ka dib uusan port 53 packet muuqan, waxaa la diyaarinayaa go'aan naqshadeed oo native Fadal64 DNS resolver ah, iyadoo aan hostname resolution weli lagu sheegan mid shaqaynaya.
