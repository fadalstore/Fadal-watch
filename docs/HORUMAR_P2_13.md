# HORUMAR P2.13 — DNS4 firmware behavior diagnosis

## Natiijada

P2.13 wuxuu baaray sababta hostname probe-ku u istaago ka hor `EFI_HTTP.Request`. Tijaabadu waxay kala saartay laba qaab oo EFI_DNS4 configuration ah:

1. `UseDefaultSetting=TRUE`, iyadoo aan station/subnet mapping la siin: OVMF wuxuu soo celiyey `EFI_DNS4 Configure = No mapping`.
2. DHCP station mapping (`10.0.2.15/255.255.255.0`) iyo QEMU slirp DNS server `10.0.2.3`, iyadoo `UseDefaultSetting=FALSE`: `EFI_DNS4 Configure = Success`, laakiin OVMF/QEMU implementation-ku wuxuu ku xannibmay gudaha `HostNameToIp` ka hor inta uusan loader-ku helin ama daabicin submit status. Packet capture-ku ma muujin port-53 traffic.

Tani ma xaqiijinayso DNS transport, mana caddaynayso in firmware-ka uu si sax ah u taageero DNS4 child instance-ka QEMU user networking. Isbeddelka explicit-DNS-ka lama gelin source-ka, sababtoo ah wuxuu regression ku keenay boot audit-ka isagoo ka dhigay loader-ka inuu ku xannibmo `HostNameToIp`.

## Xaqiijin

- Source-ka P2.12 ee commit `60e48ce` dib ayaa loogu celiyey; working tree-ga wuxuu ku noqday nadiif.
- P2.12 ZIP-ga iyo commit-ka package-ka `71fd49c` lama beddelin.
- Experiment-ka tijaabada ah wuxuu ku dhisnaa x86_64 GNU-EFI cross-toolchain, OVMF iyo QEMU e1000.
- `EFI_DNS4 CreateChild` iyo `OpenProtocol` way guulaysteen.
- No-default config: `Configure = No mapping`.
- Explicit mapping/config: `Configure = Success`, laakiin `HostNameToIp` blocking behavior ayaa muuqday.
- Pcap-ku wuxuu lahaa eber port-53 packets inta probe-ku socday.

## Go'aanka injineernimada

Looma sheeganayo hostname/DNS inuu shaqeeyo. P2.12 capability audit-ka ayaa weli ah baseline-ka saxda ah: child creation, `OpenProtocol`, Configure/query path diagnostics waa la diiwaangeliyey, laakiin DNS completion/transport lama xaqiijin. Tallaabada xigta waa in la sameeyo native Fadal64 DNS fallback oo leh timeout iyo packet contract, ama adapter firmware oo asynchronous call-ka ku xaddida iyadoo aan loader-ka boot-ka lagu xannibin. Taasi waxay u baahan tahay design/test gaar ah; firmware-specific config aan la xaqiijin lama gelin.
