# Roadmap-ka FadalWatch — Af-Soomaali

## Himilada mashruuca

FadalWatch waa nidaam hawlgal oo madax-bannaan oo laga dhisayo kernel, adeegyo iyo barnaamijyo FadalWatch u gaar ah. Linux/Ubuntu iyo qalabka kale ee host-ku waxay u adeegaan dhisid iyo tijaabin oo keliya; ma noqonayaan kernel, shell ama runtime-ka guest-ka. BIOS iyo UEFI waa laba jid oo boot oo kala gaar ah ilaa go'aan naqshadeed dambe wax ka beddelo.

Himiladu waa desktop FadalWatch u gaar ah oo fudud, ammaan ah, qurux iyo habayn leh, leh internet si masuuliyad leh loo tijaabiyey, luqado kala duwan, iyo dejin qofeed oo magaca iyo akoonka lagu maamulo. Waa himilo horumarineed, ma aha sheegasho ah in dhammaan sifooyinkaas maanta diyaar yihiin ama in la xaqiijiyey inuu ka sarreeyo Windows ama Linux.

## Kala-horraynta lagu taliyey

### 1. Xasilinta kernel-ka iyo kala-soocidda amniga

P2.9 wuxuu tijaabiyey in code-ka FSH ee ring-3 aanu wax ka qori karin boggiisa read-only. Tallaabooyinka kernel-ka ee xiga waa inay ilaaliyaan fail-closed, ABI-ga BIOS, xadka image-ka iyo kala madax-bannaanida UEFI/BIOS. Tijaabo kasta oo xusuus ah waa inay yeelataa caddayn QEMU ah iyo trace la baari karo.

### 2. Internet-ka browser-ka

Marka hore la caddeeyo waxa firmware-ka UEFI bixiyo; ka dib loo dhiso ama loo tijaabiyo DNS/hostname resolution iyo jid HTTP ah oo xadidan gudaha OVMF/UTM. Browser-ka hadda jira waa text-only; HTTPS/TLS, JavaScript, CSS, sawirro iyo redirects ma aha wax la qaadan karo inay shaqeeyaan. HTTPS waa in la bilaabaa oo keliya marka la diyaariyo entropy ammaan ah, certificate validation/root store, TLS parsing iyo tijaabooyin diidmo oo cad. BIOS TCP/IP/Git groundwork looma qaadan karo browser Internet oo dhammaystiran.

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
- UEFI: Fadal64 desktop, Files/System iyo browser qoraal ah oo adeegga HTTP-ga firmware-ka ku tiirsan; awoodda HTTPS/TLS iyo hostname resolution lama xaqiijin inay diyaar yihiin.
- UTM: profiles-ka BIOS iyo UEFI waa kala gaar; touch/pointer behavior waa shuruud tijaabo joogto ah.

## Tallaabada xigta ee la soo jeediyey

**P2.10:** samee audit kooban oo UEFI networking ah, kadib ku dar DNS4/hostname resolution ama caddee habka fallback-ka la taageerayo, kuna xaqiiji live HTTP fixture gudaha QEMU/OVMF. Ha u magacaabin HTTPS ilaa TLS, entropy iyo certificate validation la hirgeliyo.
