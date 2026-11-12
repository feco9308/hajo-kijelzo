# LS044Q7DH01 protokollaudit, 2026-11-12

## Eredmény

**A saját többsoros keret helyes. A soronkénti `0x00` dummy byte-ot nem
szabad eltávolítani. A TechToys többsoros kerete is helyes byte-kiosztású:
a további sorok elé írt `0x01` a kijelző számára dummy, nem új módparancs.**

A két adatfolyam ugyanazokra a címekre ugyanazokat a pixeleket írja.
Nem találtam ebben olyan nyolcbites eltolódást, amely indokolná a saját
multiline formátum javítását. Ez nem igazolja a fizikai kijelző működését.

A TechToys implementáció egészét nem minősítem a 2025-ös adatlap szerint
maradéktalanul megfelelőnek: a Clear órajelszáma, a 30 us-os indulási
várakozások és néhány garantálatlan időzítés külön megjegyzést igényel.

Ez a felülvizsgálat **nem módosít firmware-forrást, nem merge-el main-be,
és nem flashel**. A `review/sharp-protocol-reference` ág csak az auditot
és a byte-szintű ellenőrzés eredményeit tartalmazza. Nem hoztam létre
protokolljavításnak nevezett firmware-ágat egy nem igazolt hiba alapján.

## 1. Rögzített források és módszer

Elsődleges referencia a projekt gyökerében található
[Sharp LD-2025315, 2025-03-25](../../SPEC_LS044Q7DH01_LD2025315_20250325.pdf).
SHA256: `40669f99e9479d6932ab63e556a332d2db5bdcb33ff93b9ed17d7dd66e6c1672`.
Az alábbi oldalszámok a **nyomtatott** oldalak; a PDF-oldalszám ezeknél
kettővel nagyobb. Az adatlap helyi fájl, nincs feltöltve GitHubra.
A timing ábrákat teljes oldalas renderelésen is ellenőriztem.

TechToys master: `0591231aabea2be0125cf27564f55adb63af9480`, a távoli
állapot 2026-11-12-én ellenőrizve.

- [README, tesztelt típusok](https://github.com/techtoys/Arduino-Memory-LCD-Driver/blob/0591231aabea2be0125cf27564f55adb63af9480/README.md)
- [MemoryLCD.h, modell és felbontás](https://github.com/techtoys/Arduino-Memory-LCD-Driver/blob/0591231aabea2be0125cf27564f55adb63af9480/src/MemoryLCD.h)
- [MemoryLCD.cpp, teljes implementáció](https://github.com/techtoys/Arduino-Memory-LCD-Driver/blob/0591231aabea2be0125cf27564f55adb63af9480/src/MemoryLCD.cpp)

Az LS044Q7DH01 támogatása ténylegesen szerepel a forrásban, 320x240
felbontással. A checkout alapértelmezett modellje ugyanakkor LS027B7DH01,
400x240; tehát módosítás nélkül nem az LS044 konfigurációját fordítaná.
Az LS032 típus külön, 10 bites címzési ágát az LS044 elemzéséhez nem használtam.

A saját távoli ágakat `git fetch origin` után, változatlan forráson néztem át.
A byte-modellek a C ciklusokból rekonstruált adatfolyamok; az ellenőrző
dekóder a gyári ábra bitmezőit követi. Ez nem a C driver hoston futtatott
unit tesztje és nem logikai analizátoros mérés. Az elektromos jelalak,
valós élidő és LCD-oldali működés a forrásból nem igazolható.

## 2. Ágak: nem mindegy, melyiket nevezzük jelenleginek

| Ág | Tip commit | Küldési út | COM és lábak | Alapviselkedés / jelentőség |
|---|---|---|---|---|
| main | 65279da | 250 kHz HAL, 1LINE, GPIO/AF váltás, multiline | külső; PB0 EXT, PB1 DISP | Automatikus fekete/fehér teszt, régi kiindulás |
| fix/minimal-spi-bringup | a31a9ba | 250 kHz HAL, 2LINES, állandó AF5, multiline | külső; PB0 EXT, PB1 DISP | USB alapból OFF, egyszerű váltakozó teszt |
| test/minimal-black-white | 7d83e8c | 1 MHz HAL, 2LINES, AF5, multiline | külső; PB0 EXT, PB1 DISP | 2 s fekete / 2 s fehér, timer indul Clear előtt |
| feat/usb-manual-control | d41c8f0 | 1 MHz HAL, 2LINES, AF5, multiline | EXTMODE LOW; PB0 EXT LOW, PB1 DISP | M1 szerviz és előzetes határidő-őr, USB kézi vezérlés |
| test/swapped-pins-stripes | 1453f6f | 1 MHz HAL, 2LINES, AF5, multiline | külső; PB1 EXT CH4, PB0 DISP | Automatikus fehér/fekete/csík/inverz sorozat |
| test/line-by-line | a1df12d | 1 MHz HAL, 2LINES, AF5; egy sor és multiline API is van | külső; PB1 EXT CH4, PB0 DISP | A főprogram egyenként írja a sorokat, automatikus pásztázás |
| feat/usb-control-swapped-pins | 7c0b359 | 1 MHz HAL, 2LINES, AF5, multiline | EXTMODE LOW; PB1 EXT LOW, PB0 DISP | A tip már SPI-s M1-es változat! A branch neve önmagában félrevezető |
| fix/serial-com-actual-wiring | 5a4ef64 | 1 MHz HAL, 2LINES, AF5, multiline | EXTMODE LOW; PB0 EXT LOW, PB1 DISP | M1 kézi mód mellett is, USB auto/stop külön parancsra |
| test/techtoys-external-com | 6091d85 | 1 MHz HAL, 2LINES, AF5, multiline | külső; PB0 EXT CH3, PB1 DISP | Két Clear, DISP, timer; kézi USB mód |
| test/slow-single-line | b6cd85b | 250 kHz HAL, 2LINES, AF5, minden sor külön SCS | külső; PB0 EXT CH3, PB1 DISP | USB és ST-LINK RAM-parancsok, kézi alapmód |
| test/gpio-spi-isolation | a9b6d28 | **SPI1 tiltva**, PA5/PA7 GPIO bitküldés, minden sor külön SCS | külső; PB0 EXT CH3, PB1 DISP | Legújabb kísérleti tip; nem hardveres SPI-s véglegesítés |

A csak helyi `test/pre-m1-comparison` ág `82fb80e`-re mutat. Ez a későbbi
M1-módosítás előtti, külső COM-os, 1 MHz-es, PB0 DISP / PB1 EXT változat;
nem azonos a `feat/usb-control-swapped-pins` mai tipjével. Ezt a történeti
állapotot is ellenőriztem.

**Legfejlettebb használható hardveres SPI-s alap:**
`test/slow-single-line`, `b6cd85b14e16d0d85cace727f406fa63299c485b`.
USB, SWD-parancsok, 2LINES, állandó AF5 és kézi alapmód együtt itt vannak.
Ez szolgáltatásbeli kiindulási alap, nem bizonyítottan működő LCD-driver.

**A saját multiline implementáció legfrissebb külső COM-os alapja:**
`test/techtoys-external-com`, `6091d85c460e59b2bfe600c5ae060931d22aa1c7`.
A b6cd85b és a9b6d28 `screen()` már nem egyetlen multiline tranzakciót küld;
240 külön `sharp_write_line()` hívást használ. A kérésben idézett multiline
séma a régebbi ágakra helyes, a legújabb kettőre már nem.

## 3. Gyári módmező, bitirány, cím és pixel

Sharp 6-6-1, 31. oldal, Figure 6-6-1; 6-6-2, 33. oldal, Figure 6-6-2:

```text
Időrendi bitküldés:
M0 M1 M2 dummy dummy dummy dummy dummy
AG0 AG1 AG2 AG3 AG4 AG5 AG6 AG7
D0 D1 ... D319
```

LSB-first esetén a módbyte `M0 | (M1<<1) | (M2<<2)`. Nincs szükség
utólagos bitfordításra. M0 a byte bit0-ja, M1 bit1, M2 bit2.

| Művelet | M0 | M1=0 esetén byte | M1=1 esetén byte | M2 |
|---|---:|---:|---:|---:|
| Pixelírás | 1 | 01 | 03 | 0 |
| Hold / COM szerviz | 0 | 00 | 02 | 0 |
| All Clear | 0 | 04 | 06 | 1 |

EXTMODE=HIGH esetén M1 tetszőleges; a saját külső COM-os ágak nullán hagyják.
EXTMODE=LOW esetén M1 az egész COM-polaritás állapota, nem pixelenkénti
inverzió. A további sorok dummy mezőjének bit1-je nem friss M1-parancs.

A sorcímek 1..240, nem 0..239. A 42-43. oldali Table 6-8-1 mind a 240
bejegyzésén ellenőriztem: `sum(AG[k]<<k) == GL`. A cím a módbyte után
következik; későbbi soroknál a köztes 8 dummy órajel után. A pixelbyte
bit0-ja a byte első pixele. `1=fehér`, `0=fekete` mindkét implementációban.

## 4. A: All Clear, pontos bitek és órajelszám

Sharp 6-6-4, 36. oldal, Figure 6-6-4: három módclock, utána az ábra
**„More than 13clk”** adatátviteli periódust jelöl.

| Változat | Byte-ok M1=0 mellett | Clock összesen | Clock a 3 módclock után | Értékelés |
|---|---|---:|---:|---|
| Saját, minden auditált ág | 04 00 00 | 24 | 21 | Megfelel a szigorú olvasatnak |
| Saját soros COM, M1=1 | 06 00 00 | 24 | 21 | Ugyanúgy megfelel |
| TechToys GFXDisplayAllClear | 04 00 | 16 | 13 | Nem teljesíti a szigorúan több mint 13 feltételt |

Saját bitfolyam M1=0 mellett:

```text
04          00          00
00100000 | 00000000 | 00000000   <-- minden csoportban időrend, LSB először
^^^
M0=0, M1=0, M2=1; a többi 21 bit dummy/transfer.
```

Szigorú alsó határ 3+14=17 clock; byte-os SPI-n a saját 24 clock egyszerű
és elegendő megoldás. A Hold ábra (35. oldal) ezzel szemben 3+13=16
clockot mutat; a Clear és a Hold követelményét nem szabad összemosni.

Két 16-clockos Clear külön SCS-periódusban nem ad egy 32-clockos Clear
tranzakciót: az órajelszám követelménye tranzakciónként vizsgálandó.
A TechToys működési tapasztalata nem jogosít fel a gyári „more than”
automatikus átértelmezésére. A saját hosszabb törlését nem kell rövidíteni.

## 5. B: Egyetlen sor frissítése

A gyári 31. oldali ábra és mindkét implementáció LS044 ága ezt küldi:

| Byte index | Tartalom | Jelentés |
|---:|---|---|
| 0 | 01, soros COM M1=1 mellett 03 | 3 mód + 5 dummy bit |
| 1 | line, 1..240 | AG0..AG7 |
| 2..41 | 40 byte | D0..D319 |
| 42..43 | 00 00 | 16 végső dummy/transfer clock |

Összesen 44 byte, 352 clock, egy aktív HIGH SCS-periódus.
A TechToys UpdateLine előtti 3 us setup és utáni 1 us hold kifejezett;
a saját kódban 4 us setup, 2 us hold és legalább 2 us SCS LOW van.

Külön SCS-periódusú következő sornál új módbyte természetesen kell.
Ez különbözik a folyamatos SCS HIGH alatti multiline esettől.

## 6. C-I: Többsoros frissítés - döntés a gyári ábrából

A döntő hely: **33. oldal, Figure 6-6-2 felső sor, az első D319 utáni
„Data transfer period”**, majd ugyanott a következő `AG0 AG1 AG2...` mező.
Az ábra felirata: **„8clk [DMY] + 8clk [Address] = 16clk”**.

M0/M1/M2 kizárólag a teljes SCS HIGH elején rajzolt módmezőben szerepel.
A következő sor előtt az ábra dummy-t és címet mutat, új módmezőt nem.
A 34. oldali leírás szerint az M0 és M2 állapot az SCS LOW-ra váltásakor
törlődik. Tehát a helyes válasz **a)**: egy módmező a blokk elején.

```text
SCS HIGH
01                  módmező, M1=0
address1 + pixels1
dummy8 + address2   együtt az 1. sor 16 transfer clockja
pixels2
dummy8 + address3
...
pixelsN
dummy16            utolsó sor memóriába átadása
SCS LOW
```

TechToys `GFXDisplayUpdateBlock()` és `GFXDisplayTestPattern()` mindkettő
ismétli az `01, address, pixels40` alakot SCS HIGH alatt. Az első 01
módbyte; **a második és minden további 01 a fenti dummy8 helyére esik**.
A 34. oldali dummy-leírás szerint HIGH vagy LOW is megengedett, LOW az
ajánlott. Ezért a saját 00 és a TechToys 01 ugyanebben a mezőben egyaránt
érvényes. A forráskomment nem írhatja felül a kijelző állapotgépét.

**A saját sorvégi 00 szükséges:** köztes sornál 8 dummy clockot ad,
utána a következő cím újabb 8 clockja zárja az előző sor transferét.
Ha ezt a 00-t egyszerűen kivesszük, a következő cím a dummy helyére
kerül, és az első pixelbyte lesz a hibásan értelmezett következő cím.
Ezzel valóban elrontanánk az adatfolyamot. Új 01-et a már meglévő 00 és
cím közé beszúrni ugyanígy plusz nyolc clockot és eltolódást okozna.

### Konkrét első két sor, minden byte helye

Példa: első sor teljes fekete (00 x40), második sor teljes fehér (FF x40),
M1=0. A `byte 0` számozás a teljes közös SCS HIGH tranzakció elejétől értendő.

| Byte index | Saját multiline | TechToys multiline | Gyári szerep |
|---:|---|---|---|
| 0 | 01 | 01 | M0=1, M1=0, M2=0 + 5 dummy |
| 1 | 01 | 01 | Első sor címe: AG0=1, többi 0 |
| 2..41 | mind 00 | mind 00 | Első sor D0..D319, fekete |
| **42** | **00** | **01** | **8 dummy/transfer bit, nem új módbyte** |
| **43** | **02** | **02** | **Második cím: AG1=1; az előző sor transferének második 8 clockja** |
| 44..83 | mind FF | mind FF | Második sor D0..D319, fehér |
| 84 | 00 | 00 | Utolsó sor első 8 dummy clockja |
| 85 | 00 | 00 | Utolsó sor második 8 dummy clockja |

```text
Saját:    01 01 [00 x40] 00 02 [FF x40] 00 00
TechToys: 01 01 [00 x40] 01 02 [FF x40] 00 00
                           ^ egyetlen megengedett dummy-különbség
```

Az összes 86 byte külön sorban, funkcióval és pixel-bitindexszel:
[first-two-lines.csv](protocol-reference/first-two-lines.csv).
Az 01 byte időrendben `1 0 0 0 0 0 0 0`, a 02 cím `0 1 0 0 0 0 0 0`.

Ha a két sort a legújabb ágakhoz hasonlóan külön tranzakciókban küldjük:

```text
SCS HIGH: 01 01 [00 x40] 00 00 :SCS LOW
SCS HIGH: 01 02 [FF x40] 00 00 :SCS LOW
```

Ekkor 2x44=88 byte a két sor, nem 86. Ez is helyes, másik üzemmód.

## 7. D, E, J: Teljes frame és végső transfer clockok

N sor egyetlen multiline SCS-periódusban: `42*N+2` byte, `336*N+16` clock.
A TechToys `42*N+2` képlete ugyanazt adja, csak máshol áll a forrásban a
dummy byte. A saját minden sor utáni 00-ja és a végső plusz 00 együtt
az utolsó sor után is 16 dummy clockot eredményez.

| Eset | Byte | Clock | Tiszta clockidő |
|---|---:|---:|---|
| 1 sor | 44 | 352 | 352 us 1 MHz-en |
| 2 sor multiline | 86 | 688 | 688 us 1 MHz-en |
| 240 sor multiline, saját 1 MHz | 10082 | 80656 | 80,656 ms |
| 240 sor multiline, TechToys 2 MHz | 10082 | 80656 | 40,328 ms |
| 240 sor multiline, régebbi saját 250 kHz | 10082 | 80656 | 322,624 ms |
| 240 külön sor, b6cd85b 250 kHz | 10560 | 84480 | 337,920 ms |

A szoftveres szünetek és COM-őrök ehhez hozzáadódnak. GPIO SPI-n a fenti
hardveres frekvenciákat nem szabad a tényleges clockként feltüntetni.

240 soros multiline pontos vége, nullától számolt byte-indexek:

- utolsó (240.) sor címe: byte **10039**, értéke **F0**;
- utolsó sor 40 pixelbyte-ja: **10040..10079**;
- utolsó két dummy byte: **10080 és 10081**.

A két teljes keret kizárólag a `42,84,...,10038` indexű **239 köztes dummy
byte-on** tér el. Ezek nem pixelbyte-ok. Az összes cím és pixel azonos.
A teljes frame két végső dummy byte-ját egyikből sem szabad kihagyni.

## 8. F: SCS, clock és nyugalmi szintek

Sharp Table 6-3-1, 24. oldal: SCS setup >=3 us, hold >=1 us, LOW >=1 us;
adatfrissítésnél HIGH >=180 us, Holdnál HIGH >=12 us. SCLK maximum 2 MHz,
tipikusan 1 MHz, megadott minimum frekvencia nincs. SCLK HIGH és LOW >=200 ns.
A SI setup minimum 120 ns, hold 190 ns. Az élidők csak méréssel igazolhatók.

Saját hardveres driver: PA4 GPIO, aktív HIGH; begin előtt LOW, 2 us LOW,
4 us setup; sikeres blocking adás után 2 us hold, LOW, 2 us LOW.
Egy 1 MHz-es sor 352 us, a 250 kHz-es sor 1408 us tiszta clockidővel is
teljesíti a 180 us minimumot. A 24-clockos parancs 1 MHz-en 24 us plusz
setup/hold, nem túl rövid. Nincs hardveres aktív-LOW NSS.

TechToys UpdateLine/UpdateBlock: 3 us setup és 1 us hold kifejezett.
2 MHz-en a sor 176 us, setup+hold minimumokkal 180 us. SCS LOW időt a HAL
nem külön késleltet, a függvényhívások járulékos idejére támaszkodik;
így az 1 us LOW minimum minden platformon nem bizonyítható a forrásból.
A TestPattern végén külön `hal_delayUs(1)` sincs: a lezáró GPIO/SPI
hívások ideje adhat elég holdot, de erre nincs platformfüggetlen garancia.

A TechToys TestPattern középen opcionális callbacket futtat SCS HIGH
mellett. Ennek ideje korlátlan lehet; használata előtt a COM-időzítést
külön ellenőrizni kell. Teljes frame-re a 20 Hz frissítési maximum is
érvényes. A 2 MHz-es 40,328 ms clockidő önmagában nem garantál >=50 ms
frame-periódust, ha a felhasználó korlátlanul ismétli a függvényt.
Ez hívóoldali feltétel, nem bizonyítottan fennálló hiba minden példában.

Kommunikáción kívül a Sharp 21-22. oldala SCS/SCLK/SI LOW-t kér.
A saját hardveres ágakban CPOL=0 és a legutolsó nullás dummy biztosítja
az előírt logikai állapotot a tervezett működésben; PA5/PA7 AF5 marad.
A GPIO-s kísérleti ágban a clock végén SCLK LOW, a végső nulla miatt SI LOW.
A TechToys is nullával zár; a konkrét Arduino SPI-pad nyugalmi viselkedése
platformfüggő. Reset alatti Hi-Z állapotot egyik forrás sem old meg
külső lehúzások nélkül.

## 9. G: COM - külön probléma a pixelstreamtől

Sharp 28-30., 37-39. oldal:

- EXTMODE HIGH: külső EXTCOMIN, M1 tetszőleges. A referencia ezt valósítja meg.
- EXTMODE LOW: EXTCOMIN LOW, periodikus M1-frissítés képtartás közben is.
  A TechToysban ehhez nincs soros COM-szerviz.
- A gyári 28. oldal szerint **1 Hz EXTCOMIN mellett fCOM=0,5 Hz**, nem 1 Hz.
  Minden felfutás állapotváltást okoz; teljes COM-periódus két felfutás.
- Ha M1-et 500 ms-onként váltjuk, a teljes M1/COM-periódus 1 s; ez más
  fCOM, mint az 1 Hz-es külső EXTCOMIN-é. A két korábbi próba tehát nem
  teljesen azonos COM-frekvenciát használt.
- SCS LOW alatt EXTCOMIN felfutásra közvetlen COM-váltás történik. HIGH
  alatt az él a COM-flaget váltja, az alkalmazás az SCS lefutásához kötődik.
  A gyár állandó COM-periódust kér, és tiltja a több külső felfutást egy
  HIGH ablakban. Hosszú vagy változó szünet ezt befolyásolhatja.

A saját multiline külső ágak a CNT502..800 ablakban kezdenek; 1 MHz-en
a kb. 81 ms átvitellel marad tartalék a következő CNT500 felfutásig.
A 250 kHz-es régi teljes frame is kb. 323 ms, a legalább 700 ms tartaléknál
rövidebb. A b6cd85b külön sorai a CNT497..501 környéket kerülik; a GPIO-s
ág hosszabb, CNT490..501 őrt használ. Ezek forrásból számolt névleges
idők; szélsőséges IRQ-latencia vagy jelalak nem igazolt.

A későbbi 7c0b359/5a4ef64 M1-poll a teljes frame utánig késleltetheti a
következő váltást (kb. 81 ms plusz futási idő). A korábbi d41c8f0 ehhez
előzetes határidő-őrt is tartalmaz. A későbbi egyszerűsítés így nem
automatikusan szigorúbb időzítésű. Ez COM-jitter kockázat, **nem** egy
eltolt gate address bizonyítéka és nem magyarázza bizonyítottan a hibás oszlopot.

## 10. Power-on: kért háromoszlopos összevetés

A saját oszlop az a9b6d28 jelenlegi tip és a b6cd85b hardveres alap közös
indulását mutatja. SPI szállításuk különbözik, sorrendjük azonos.

| TechToys | Saját aktuális indulás | Sharp LD-2025315 előírás / értékelés |
|---|---|---|
| BSP: DISP/SCS/EXT LOW | GPIO kimeneti latches LOW beállítása a módváltás előtt | DISP LOW inicializálás alatt; jelek ne lebegjenek |
| Kapcsolható 5 V és 10 ms várakozás a forrásban kommentelve | Nincs tápkapcsoló/power-good, 100 ms szoftveres várakozás | VDD/VDDA legyen stabil; VDD>=VDDA. A kód ezt vissza nem méri |
| All Clear 04 00 | All Clear 04 00 00 | Pixelmemória inicializálása; szigorú Clear-clock feltétel saját oldalon teljesül |
| Újabb All Clear | Újabb All Clear | 22. oldal: legalább egyszer, vagy teljes fehér pixelírás. Kettő nem kötelező |
| Clear után nincs kifejezett 100 us guard | Clear után 50 ms | TA >=100 us. Saját megfelel, TechToys forrásból nem garantált |
| DISP HIGH | DISP HIGH | Inicializálás után engedélyezni |
| 30 us | 1 ms | T3 >=100 us; saját várakozás tartalékos |
| EXTCOMIN timer start | PB0 AF2 TIM3_CH3: 1 Hz, 50%, kezdetben LOW | Külső COM csak HIGH EXTMODE-nál; T3 ténylegesen az első élhez viszonyítandó |
| 30 us, majd normál hívó folytatás | Első felfutás CNT500, majd várakozás CNT502-ig | T4 >=100 us az inicializáló COM-él után; saját 2 ms névleges guard |
| A következő SPI-hívás nincs első COM-élhez kapuzva | Következő frissítés csak a várakozás után | A TechToys két 30 us értéke nem igazolja a 2025-ös sorrendi minimumokat |
| Pixelmemória törölve marad a következő alkalmazásparancsig | Manuális alapmód, nincs automatikus képváltás induláskor | Képtartáskor SCS LOW, külső COM folyamatosan futhat |

A TechToys timer kezdeti állapota és első élének ideje platformfüggő;
nem állítom, hogy az első él biztosan pontosan 30 us-nál történik. A gond
az, hogy a forrás a 100 us minimumokat és az első él utáni őrt nem garantálja.
Az off/on parancs nem tápki-/bekapcsolás; a két folyamatot külön kell kezelni.

A main és a fix/minimal ág még Clear -> timer -> 1 ms -> DISP sorrendet
használ. A minimal-black-white és a swapped/line-by-line ágakban a timer
már a Clear előtt elindul. A gyári TB1 megengedi az EXTCOMIN-t DISP LOW
alatt is, de ilyenkor nincs tényleges COM-polaritásváltás; a DISP utáni
első használható él és a T4 guard továbbra is számít. A későbbi külső
ág ezt kifejezetten megvárja.

## 11. Két All Clear: tapasztalati kerülőmegoldás

TechToys `GFXDisplayPowerOn()` (73-85. sor) kétszer hívja a törlést, és
kommentben jelzi, hogy egy törlés náluk nem volt mindig elég.
A gyári 22. oldal T2 pontja **„1 time or more”** inicializálást ír elő,
vagy teljes képernyős fehér pixelírást. Nem kötelező a két Clear.

Kontrollált tesztként érdemes volt kipróbálni, de **már kipróbáltuk**:
e51c6d4/6091d85 óta a későbbi külső COM-os ágak kettőt küldenek. A hozzájuk
tartozó korábbi felhasználói megfigyelés továbbra is csíkos/villogó képet
jelzett. Ez nem laboratóriumi gyökérok-bizonyíték, de nem indokolt új
javításként eladni egy már jelen lévő második Clear-t.

## 12. Több HAL_SPI_Transmit egy SCS HIGH alatt

Vizsgált, ténylegesen rögzített HAL: STM32G4xx_HAL_Driver
`a6001282dfacfff57e9710250f15e4333b578865`.
[Gyártói forrás](https://github.com/STMicroelectronics/stm32g4xx-hal-driver/blob/a6001282dfacfff57e9710250f15e4333b578865/Src/stm32g4xx_hal_spi.c).

`HAL_SPI_Transmit()` a normál, sikeres **2LINES master** útvonalon:

1. A handle állapotát BUSY_TX-re állítja és zárolja.
2. Az SPI letiltás/irányállítás csak az 1LINE feltételes ágban van.
3. SPE-t csak akkor engedélyezi, ha eddig tiltva volt; siker végén nem tiltja.
4. 8 bites DataSize esetén byte-okat, részben kétbyte-os FIFO-packinggel ír.
5. A végén megvárja az üres TX FIFO-t és BSY=0-t, üríti a RX FIFO-t,
   2LINES módban törli az elvárt RX overrun flaget, READY-re vált és oldja a lockot.

Nincs normál 2LINES végén SCK/GPIO újrakonfigurálás vagy automatikus
NSS-impulzus. NSS_SOFT, NSSP tiltva, CRC tiltva. A következő HAL-hívás
ugyanazt az SPI-konfigurációt és LSB-first bitirányt használja.

A saját régi multiline `screen()` 1+240+1=242 HAL-hívást végez, de
egyazon SCS HIGH alatt; a byte-sorozat ettől nem változik. HAL-híváshatár
nem LCD-protokollhatár, csak az SCS az. A packingból a forrás szerint
nem következik beillesztett vagy elhagyott bit. Az utolsó egybyte-os
hívást a HAL külön byte-os DR-írással kezeli, nem küld automatikus plusz byte-ot.

Lehetnek rövid interbyte/intercall szünetek; USB IRQ is nyújthatja azokat.
A gyári 34. oldal folyamatos adatbevitelt kér, de a 24. oldali táblázat
nem ad meg maximális SCLK LOW időt vagy maximális byte-közi szünetet.
Egy rövid szoftverszünetből ezért nem bizonyítható protokollsértés vagy
biteltolódás. A TechToys maga is byte-onként hívja az Arduino SPI-t.
Hosszú szünet azonban belenyúlhat a COM-élbe, ezért azt külön kell kezelni.

A kezdeti main **1LINE** konfigurációjában valóban van minden HAL-hívás
elején SPE tiltás és BIDIOE állítás. Ez indokolja a már meglévő 2LINES
változat előnyben részesítését. Nem bizonyítja, hogy a régi villogást
ez okozta. Új biteltolódást pusztán a 242 hívásra hivatkozva nem állítok.

Elektromos glitchre, valódi clocktöbbletre vagy zavarérzékenységre csak
a forrás alapján nem lehet teljes garanciát adni. Itt nem történt mérés.

## 13. Mit magyaráznak és mit nem magyaráznak a különbségek?

| Kérdés | Auditkövetkeztetés |
|---|---|
| Saját multiline helyes? | Igen, a vizsgált ágak formátuma megfelel a Figure 6-6-2-nek |
| TechToys multiline helyes? | Igen, az LS044 ág byte- és bitkiosztása helyes; ez nem az egész driver időzítési megfelelőségének igazolása |
| A saját 00 eltol egy címet? | Nem, szükséges köztes dummy. Eltávolítása rontaná el a címezést |
| Egy invertált pixeloszlop oka a dummy-különbség? | A dekódolt pixelek azonosak, ezért ebből a különbségből nem vezethető le |
| Kimaradó pixelek oka a két frame-formátum eltérése? | Nem találtam erre forrásból bizonyított okot |
| Hibás bitirány elméletben okozhat képhibát? | Igen, de a saját hardveres ágak helyesen LSB-firstre vannak állítva; egyszerű bitfordítás a teljes 00/FF képet sem változtatná meg |
| M1 önmagában egy adott oszlopot invertál? | A protokollban nem, COM-állapotot állít; COM-hiba látható hatását ettől még mérés nélkül nem zárom ki |
| Két Clear kötelező? | Nem, gyári minimum egy; TechToys workaround, a későbbi saját ágakban már megvan |
| COM és pixelstream külön kezelendő? | Igen. Külön beavatkozással, rögzített bekötés és frekvencia mellett |

A fotó és videó önmagában nem választja szét a clock/adat eljutását,
a COM-fázist, a táp hatását és a panel belső működését. A programban
kiolvasott HAL_OK nem LCD-visszaigazolás. A `disp=1` MCU-oldali jelérték,
az `extmode=1` a firmware konfigurációja, nem a fizikai 8-as láb mérése.
Az, hogy off állapotban fehér a panel, önmagában nem bizonyít sikeres Clear-t
vagy fehér pixelírást: a DISP LOW eleve fehér megjelenítést kér.

## 14. Build és ellenőrzések - nem történt feltöltés

Két friss, elkülönített CMake/Ninja build, ugyanazokkal a rögzített
submodule-forrásokkal, ST GCC 14.3.1, `-Wall -Wextra -Werror`:

| Forrás | Eredmény | Compiler warning | Flash | SRAM, stackfoglalással |
|---|---|---:|---:|---:|
| b6cd85b, test/slow-single-line, hardveres SPI | PASS | **0** | 26140 B / 128 KiB | 5504 B / 22 KiB |
| a9b6d28, test/gpio-spi-isolation, legújabb diagnosztika | PASS | **0** | 24980 B / 128 KiB | 5504 B / 22 KiB |

A többi ág forrásauditot kapott, nem állítok mindegyikre új buildet.
A firmware-forrás és a fordítási kapcsolók nem változtak.

STM32G431CBU6 ellenőrzött projektkonfiguráció: STM32G431xx define,
gyártói startup_stm32g431xx.s, 128 KiB Flash a 0x08000000 címen,
22 KiB összefüggő SRAM a 0x20000000 címen, kezdeti SP 0x20005800,
2 KiB stackfoglalás. A külön 10 KiB CCM nincs használatban. Mindkét friss
bináris első vektora a várt SP; Reset_Handler a Flash-ben, Thumb bit beállítva.

A firmware-ek nem a `build-st` aktív képfájljait írják felül, hanem
`tmp/protocol-audit/build-hal` és `build-latest` könyvtárban készültek.
Nem reseteltem vagy vezéreltem a csatlakoztatott MCU-t ebben az auditban.

A byte-modellek eredménye: [stream-checks.json](protocol-reference/stream-checks.json).
1/2/240 sor: PASS; hiányzó dummy, extra byte és hibás bitfordítás negatív
esetei elutasítva; mind a 240 gyári cím ellenőrzése PASS. Ez byte-modell
ellenőrzés, nem a fordított C driver végrehajtása és nem fizikai SPI-capture.

## 15. Csak az audit után javasolt következő fejlesztés

**Nincs igazolt saját multiline hiba, így ezen a címen nem készítettem
firmware-javítást.** A kérés feltételes új tesztprogramját sem állítom
elkészültnek: a következő külön, indokolt fejlesztés lehetne a b6cd85b
hardveres SPI-s alapból, a következő rögzített tesztfelülettel:

1. Safe GPIO, stabil táp feltétele, jelenlegi tartalékos power-on időzítések.
2. Clear, majd explicit teljes fehér pixelírás; nincs automatikus képcserélés.
3. Legalább 30 s fehér megfigyelési ablak; status/help elérhető, képírási
   parancsot addig visszautasít vagy későbbre kér. A külső COM folyamatos.
4. USB `black` után a kép marad, amíg új kifejezett képírási parancs nem jön.
5. Új, külön `stripes1` parancs 1 pixeles függőleges csíkokkal (pl. AA x40).
6. Új `P1`..`P320` parancsok fehér alapon egy fekete oszlophoz; opcionális,
   külön elindított mozgó oszlopteszt, stop után álló kép. Nem fekete/fehér
   villogtatás és nem automatikus indulási pásztázás.

Ehhez nem kell framebuffer: 40 byte-os sort lehet ismételni. LSB-first
esetén az egyetlen fekete oszlop byte-indexe `(P-1)/8`, bitje `(P-1)%8`;
kezdő fehér FF byte-ból ezt a bitet kell nullázni. P1 -> első byte FE,
P8 -> első byte 7F, P9 -> második byte FE, P320 -> utolsó byte 7F.
A jelenlegi `pattern` 8 pixeles sávokat ad, nem ezt az új tesztet.

Megőrzendő: SPI_DIRECTION_2LINES, mode0, LSB-first, PA5 SCLK, PA7 MOSI,
PA4 aktív HIGH GPIO SCS, állandó PA5/PA7 AF5, nincs normál GPIO/AF kapcsolgatás.
A GPIO-s a9b6d28 csak elkülönítő kísérlet; nem ilyen célú firmware-alap.
A sorok közötti meglévő dummy-t nem kell megváltoztatni.

## 16. Fizikai bekötés az első későbbi külső COM-os teszthez

Ez a b6cd85b hardveres alapnak megfelelő bekötés. Az audit nem kér most
átkötést és nem indít tesztet; a legutóbbi egyeztetés szerint már HIGH
EXTMODE van bekötve. A kérésben említett GND-s javulás korábbi megfigyelés.

| LCD csatlakozó | Név | Bekötés |
|---:|---|---|
| 1 | SCLK | PA5, SPI1 AF5 |
| 2 | SI | PA7, SPI1 AF5 |
| 3 | SCS | PA4, normál GPIO, aktív HIGH |
| 4 | EXTCOMIN | PB0, AF2 TIM3_CH3, 1 Hz, 50% |
| 5 | DISP | PB1 GPIO, induláskor LOW, inicializálás után HIGH |
| 6 | VDDA | 5 V |
| 7 | VDD | 5 V |
| 8 | EXTMODE | VDD / 5 V, rögzítve |
| 9 | VSS | közös GND |
| 10 | VSSA | közös GND, VSS-szel összekötve a csatlakozónál |

Az STM32 és LCD földje közös. Az LCD számozását a panel csatlakozójának
1-es jelölésétől kell követni, nem a nyák fotón látható bal/jobb oldalából.
Reset idejére PA4/PA5/PA7/PB0/PB1 jelekhez kb. 10 kohm külső lehúzás
ajánlott; a működési 3,3 V GPIO-jeleket nem kell emiatt 5 V-ra felhúzni.
EXTMODE-ot csak tápmentesen szabad váltani. GND-s EXTMODE-hoz külön M1-es
firmware tartozik, nem a fenti timeres külső mód.

**Végső döntés:** a saját multiline byte-formátumot meg kell tartani.
A TechToys referenciából nem következik olyan új módbyte vagy dummy-törlés,
amellyel megalapozottan kijavítható volna a megfigyelt oszlop- vagy pixelhiba.
A következő programváltoztatás célzott diagnosztika lehet, nem bizonyított
protokolljavítás. Feltöltéshez külön felhasználói utasítás szükséges.
