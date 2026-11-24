# Roadmap-ka FadalWatch — Af-Soomaali

## Himilada mashruuca

FadalWatch waa nidaam hawlgal oo madax-bannaan oo laga dhisayo kernel, adeegyo iyo barnaamijyo FadalWatch u gaar ah. Linux/Ubuntu iyo qalabka kale ee host-ku waxay u adeegaan dhisid iyo tijaabin oo keliya; ma noqonayaan kernel, shell ama runtime-ka guest-ka. BIOS iyo UEFI waa laba jid oo boot oo kala gaar ah ilaa go'aan naqshadeed dambe wax ka beddelo.

Himiladu waa desktop FadalWatch u gaar ah oo fudud, ammaan ah, qurux iyo habayn leh, leh internet si masuuliyad leh loo tijaabiyey, luqado kala duwan, iyo dejin qofeed oo magaca iyo akoonka lagu maamulo. Waa himilo horumarineed, ma aha sheegasho ah in dhammaan sifooyinkaas maanta diyaar yihiin ama in la xaqiijiyey inuu ka sarreeyo Windows ama Linux.

## Kala-horraynta lagu taliyey

### 1. Xasilinta kernel-ka iyo kala-soocidda amniga

P2.9 wuxuu tijaabiyey in code-ka FSH ee ring-3 aanu wax ka qori karin boggiisa read-only. Tallaabooyinka kernel-ka ee xiga waa inay ilaaliyaan fail-closed, ABI-ga BIOS, xadka image-ka iyo kala madax-bannaanida UEFI/BIOS. Tijaabo kasta oo xusuus ah waa inay yeelataa caddayn QEMU ah iyo trace la baari karo.

### 2. Internet-ka browser-ka

P2.10 wuxuu xaqiijiyey DHCP iyo plaintext HTTP GET ku socda numeric IP oo gaaray fixture maxalli ah oo xaddidan. OVMF/QEMU-gu wuxuu bixiyey HTTP Service Binding iyo EFI_HTTP child inkastoo `LocateProtocol(EFI_HTTP)` uusan helin protocol-ka. P2.10 hostname probe-gii hore wuxuu ku fashilmay `EFI_HTTP.Configure` (`Already started`, `stage=4`); taasi ma ahayn caddayn DNS failure. P2.11 wuxuu saxay Configure-once: laba numeric-IP GET oo xiriir ah waxay gaareen fixture-ka, labaduna marker-kooda way soo celiyeen. Markii hostname-ka dib loo tijaabiyey, `EFI_HTTP.Request` wuxuu soo celiyey `Not Found` (`stage=5`, `HTTP=0`), fixture-kuna ma arag request hostname cusub. Pcap-ku ma lahayn port 53 traffic ama TCP session hostname cusub; sidaas darteed sababta DNS/transport wali lama kala saarin. DNS4 Service Binding waa la helay, balse protocol instance lama helin; abuurista child-ka iyo `HostNameToIp` wali lama tijaabin. Internet guud lama tijaabin. Browser-ku waa text-only; HTTPS/TLS, JavaScript, CSS, sawirro iyo redirects lama hirgelin. HTTPS waa in la bilaabaa oo keliya marka la diyaariyo entropy ammaan ah, certificate validation/root store, TLS parsing iyo tijaabooyin diidmo oo cad. Faahfaahinta: [HORUMAR P2.10](HORUMAR_P2_10.md), [HORUMAR P2.11](HORUMAR_P2_11.md) iyo [qorshaha Phase 2](PHASE2_PLAN.md).

### 3. Aqoonsi iyo password ammaan ah

Ka hor samaynta account wizard, waa in la naqshadeeyo xuduudaha akoonnada: aqoonsi u gaar ah user kasta, kala-soocidda rukhsadaha, password hashing leh salt iyo hab ku habboon password-ka, xaddididda isku-dayada, beddelidda password-ka, iyo soo-kabashada akoonka. Password plaintext ah laguma kaydin karo FAT/ESP. Maaddaama disk-ga UTM aanu hadda lahayn keyd sirgashan, waa in la caddeeyo xogta si dhab ah loo ilaalin karo iyo waxa u baahan disk encryption ka hor inta aan xog gaar ah la ballanqaadin.

BIOS FSH wuxuu hore u leeyahay login xaddidan iyo beddelidda password-ka; taasi ma aha multi-user security buuxda. UEFI desktop-kana weli looma qaadan karo inuu leeyahay account/login system.

### 4. Luqadaha iyo qaabka qoraalka

Marka hore la sameeyo qaab locale ah oo strings-ka UI-ga laga sooco code-ka, UTF-8 la taageero, lana qeexo font/rendering. Kadib waxaa lagu bilaabi karaa Somali iyo English; luqado kale waxaa lagu daraa marka font, keyboard layout, taariikh/waqti iyo turjumaad la tijaabiyo. Arabic iyo luqadaha RTL waxay u baahan yihiin bidix-midig/ midig-bidix (bidi), shaping iyo cursor behavior gaar ah; taageero Unicode la'aan lama sheegan karo luqad caalami ah.

### 5. Dejinta koowaad iyo shakhsiyeynta

Marka akoonka iyo locale-ku ammaan u noqdaan, waxaa la naqshadayn karaa first-run setup oo u oggolaanaya qofka inuu doorto luqadda, magaca muuqda, username, password iyo doorashooyinka aasaasiga ah ee desktop-ka. Wax kasta oo la kaydiyo waa inay yeeshaan xuduud cad oo oggolaansho iyo dib-u-dejin leh; ha gelin xog qofeed ama password dhab ah repository-ga.

### 6. Desktop-ka iyo UTM

Fadal64 ha yeesho muuqaal u gaar ah oo nadiif ah, taskbar, daaqado si joogto ah loo maamulo, Start/apps cad, fonts la akhrin karo, iyo jawaab degdeg ah. UTM/UTM SE waa bartilmaameedka: mouse, keyboard, trackpad iyo touch waa in mid kasta si gaar ah loo tijaabiyaa si cursor-ku aanu u boodboodin, taabasho la hayo aanay u noqon clicks soo noqnoqda, focus-kuna u caddaado. Muuqaalka waa in laga dhisaa fikrado desktop guud, ee aan lagu koobiyeyn nidaam kale.

### 7. Xirmooyin iyo sii-dayn

BIOS iyo UEFI UTM profiles-ku gooni ha ahaadaan. Isbeddel kasta oo sii-dayn ah wuxuu u baahan yahay image la boot-gareeyey, regression tests, checksums iyo qoraal Somali ah. ZIP-yada Phase 1, P2.6 iyo P2.8 waa taariikhi/frozen; sii-dayn cusub waa in loo sameeyaa magac iyo checksum cusub, ee aan lagu dul-qorin kuwii hore.

## Xaaladda hadda

- BIOS: kernel 32-bit freestanding ah, FSH ring-3 shell, FAT12/ATA, networking foundation iyo P2.9 write-protection negative control.
- UEFI: Fadal64 desktop, Files/System iyo browser qoraal ah oo adeegga EFI_HTTP firmware-ka ku tiirsan. DHCP iyo laba sequential numeric-IP HTTP fixture GET waa la xaqiijiyey. DNS4 Service Binding waa la helay laakiin EFI_DNS4 protocol instance lama helin; hostname probe-gu wali ma gaarin fixture-ka, sidaas darteed DNS/hostname lama xaqiijin. Internet guud iyo HTTPS-na lama tijaabin.
- UTM: profiles-ka BIOS iyo UEFI waa kala gaar; touch/pointer behavior waa shuruud tijaabo joogto ah.

## Tallaabada xigta ee la soo jeediyey

**P2.11 (qaybta session reuse waa la xaqiijiyey; hostname diagnosis way socotaa):** labada sequential numeric-IP GET waxay fixture-ka gaareen oo marker-kooda soo celiyeen. Hostname probe-ga ka dib hagaajintu wuxuu ku dhammaaday `EFI_HTTP.Request` → `Not Found` (`stage=5`, `HTTP=0`); fixture-ku ma helin hostname request. QEMU pcap-ku ma muujin port 53 ama hostname TCP; DNS4 Service Binding waa la helay, laakiin `EFI_DNS4` protocol instance lama helin. Tallaabada xigta waa in firmware DNS4 child la abuuro, la configure-gareeyo, `HostNameToIp`-na si gooni ah loo tijaabiyo. Ilaa taas la helo, hostname/DNS lama xaqiijin; Internet guud iyo HTTPS lama sheeganayo. Faahfaahinta: [HORUMAR P2.11](HORUMAR_P2_11.md).
