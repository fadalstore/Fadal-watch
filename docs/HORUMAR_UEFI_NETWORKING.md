# Horumarka shabakadda UEFI ee FadalWatch

> **Fiiro taariikhi ah — 24 Oktoobar 2026:** Warbixintani waxay diiwaangelinaysaa natiijadii TCP4 fallback ee workspace-kii hore. Code-ka fallback-ka lama darin GitHub `main` ee hadda jira (`d270cfe`), sidaas darteed ha loo qaadan in fallback-ku ku jiro build-ka hadda. Faahfaahinta dib-u-eegista iyo qorshaha port-gareynta waxay ku jiraan [warbixinta P2.8](HORUMAR_P2_8.md).

## Xaaladda tijaabada: 24 Oktoobar 2026

### TCP4 HTTP download

Cilladdii hore loogu arkay heerka 13 ma ahayn cillad DHCP ama IPv4 mapping ah. QEMU/OVMF wuxuu si guul leh u helay lease-ka, TCP4-na wuxuu gaaray adeegga HTTP. Khaladku wuxuu ku jiray heshiiska soo-celinta callback-ka kaydinta: `fadal_home_write()` wuxuu `FADAL_HOME_OK` ugu soo celiyaa `0`, halka Browser-ku `0` u fasirayo download fashilmay. Sidaas darteed, jawaabtii server-ka waa la kaydin karay laakiin Browser-ku ma aqoonsanayn guusha.

`kernel/uefi/tcp_http.c` ee workspace-kii hore waxaa lagu saxay in la kala saaro natiijada kaydinta iyo tirada bytes-ka la qoray. Haddii qoristu guulaysato, TCP4 client-ku wuxuu soo celiyaa dhererka body-ga HTTP-ga. Haddii kaydintu fashilanto, hawshu waxay ku dhammaataa `EFI_DEVICE_ERROR`; jawaab khaldan ama body aan dhammaystirnaynna waxaa loo diiwaangeliyaa heerka parser-ka.

Tijaabo toos ah ayaa lagu sameeyey QEMU/OVMF iyadoo la isticmaalayo `http://10.0.2.2:8765/`. Browser-ku wuxuu soo dejiyey faylka, waxaana bytes-ka ku jira `FADALHOME/Downloads/PAGE~1.HTM` la barbar dhigay fixture-kii HTTP; waxay ahaayeen isku mid. `make -C kernel/uefi check` sidoo kale wuu guulaystay. Regresion-test-ka Browser-ka waxaa laga saxay in uu adeegsado `--screenshot`-ka uu isticmaaluhu doortay, halkii uu ka qori lahaa `window-controls.ppm`.

### Xaddidaadaha waqtigaas la diiwaangeliyey

Fallback-kaas wuxuu aqbalaa oo keliya HTTP iyo IPv4 literal; DNS, HTTPS/TLS, JavaScript iyo sawirrada bogagga lama taageero. Jawaabta HTTP-ga waxay ku xaddidan tahay 64 KiB. UTM/OVMF-ka caadiga ah waa in mar kale lagu tijaabiyaa marka la isku daro NIC backends kala duwan.

Tijaabadu waxay xaqiijisay in codsiga HTTP la dhammaystiray, faylka la kaydiyey, isla markaana muuqaalka Browser-ku is beddelay. Ma samayn OCR ama xaqiijin pixel-level ah oo caddaynaysa in xaraf kasta oo preview-ga bogga ahi si sax ah u muuqdo.

## Tallaabooyinka port-gareynta ee harsan

1. U port-garee fallback-ka UEFI DNS4 iyo ABI-ga source-ka hadda jira; xaqiiji in OVMF/UTM uu bixinayo DNS4 protocol-ka iyo DNS server-ka DHCP.
2. Xooji tijaabada UI-ga Browser-ka si ay u xaqiijiso qoraalka preview-ga, halkii ay ka hubin lahayd oo keliya in pixels-ku is beddeleen.
3. Ku dar navigation fudud oo link-yada qoraalka ah ah.
4. Qorshee aasaaska TLS ka hor HTTPS ama Git smart-HTTP: entropy ammaan ah, kaydka CA certificates, xaqiijinta certificate-ka, iyo xuduudaha xusuusta.
5. Ka saar calaamadaha diagnostic-ka ee TCP4 kaddib marka port-gareynta iyo tijaabooyinka UTM/DNS la adkeeyo.
