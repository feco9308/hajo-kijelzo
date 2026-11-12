# Végleges adatlap-audit — 2026-11-10

Elsődleges referencia: `SPEC_LS044Q7DH01_LD2025315_20250325.pdf`,
Sharp LD-2025315, 2025-03-25. A hivatkozott oldalszámok a dokumentum
nyomtatott oldalszámai; a PDF-ben ehhez +2 adandó.
SHA256: `40669F99E9479D6932AB63E556A332D2DB5BDCB33FF93B9ED17D7DD66E6C1672`.
A timing diagramokat szövegként és renderelt képként is ellenőriztem.

## 1. SPI-bitfolyam — megfelel

6-6-1, Figure 6-6-1, 31. oldal; 6-8, 42–43. oldal:
`M0, M1, M2, 5 dummy, AG0..AG7, D0..D319, 16 dummy`.
SPI mode 0, LSB-first: a módbyte bit0-ja M0, bit1-je M1, bit2-je M2.
A címbyte bit0-ja AG0; az API címzése 1..240, a gyári címtáblával egyezik.
A pixelbyte bit0-ja az első pixel; HIGH fehér, LOW fekete.
A PDF címtáblájából kinyert mind a 240 sorcím automatikus összevetése is sikeres.
Egy sor 44 bájt = 352 órajel. Teljes képnél 6-6-2, Figure 6-6-2,
33. oldal szerinti sorfűzés: 8 dummy + következő cím 8 bit; utolsó sor után
16 dummy. Összesen 10082 bájt = 80656 órajel, névleg 80,656 ms tiszta SPI-idő.

## 2. All Clear — megfelel

6-6-4, Figure 6-6-4, 36. oldal: M0=0, M2=1, M1 külső COM mellett tetszőleges.
Küldött byte-ok: `04 00 00`, tényleges időrendi bitek:
`0 0 1 0 0 0 0 0 | 0 0 0 0 0 0 0 0 | 0 0 0 0 0 0 0 0`.
Az ábra 3 módórajel után **13-nál több** transfer-órajelet kér.
Szigorú értelmezéssel legalább 17 összes órajel kell; a 24 órajelből
21 jut a mód-bitek utáni szakaszra, tehát megfelelő tartalék van.

## 3–4. SCS és nyugalmi szintek — megfelel

6-2 TB3, 21–22. oldal; 6-3, 24–25. oldal.
Minden begin: SCS LOW, SPI kikapcsolás, SCLK/SI GPIO LOW, timerfázisra
várakozás, legalább 2 µs LOW, AF5 kiválasztása, SCS HIGH, legalább 4 µs setup.
End: sikeres HAL-adás megvárja a BSY végét, legalább 2 µs hold, SCS LOW,
SPI kikapcsolás, SCLK/SI GPIO LOW, legalább 2 µs LOW.
A gyári minimumok: SCS LOW 1 µs, setup 3 µs, hold 1 µs.
Hiba esetén a SPI leáll és a jelek LOW-ra kerülnek. Képmegtartás alatt
nincs SPI-forgalom. Reset alatt a szintekhez külső lehúzások szükségesek.

## 5. Power-up — módosítva, megfelel a szoftverrel biztosítható rész

6-2, Figure 6-2-1, 20–22. oldal:

1. DISP/SCS/SCLK/SI/EXTCOMIN kezdetben LOW.
2. Már stabil LCD-táp után 100 ms szoftveres várakozás, majd All Clear.
3. A clear befejezése után 50 ms; TA minimum 100 µs.
4. TIM3 PWM2 indítása, kezdeti LOW fázissal, DISP továbbra is LOW.
5. 1 ms várakozás, majd DISP HIGH.
6. Az első EXTCOMIN felfutás névleg a timer indulása utáni 500 ms-nál van.
   DISP HIGH-tól ehhez kb. 498–499 ms jut: T3 minimum 100 µs teljesül.
7. Az első adatfrissítés csak CNT>=502-nél, legalább 2 ms-mal az EXTCOMIN
   felfutás után indul: T4 minimum 100 µs teljesül.

A gyári TB1 megjegyzés több indítási sorrendet is enged; a módosított
változat a felhasználó által kért timerindítás → DISP HIGH sorrendet követi.
Az 50 ms biztonsági várakozás, nem a 2025-ös adatlap kötelező T2 értéke:
T2 legalább egy All Clear műveletet kér.
T1 >=2 SCLK periódus követelménye stabil táp mellett a 100 ms várakozással
bőven teljesül. A táp stabilitása nem mérhető a megadott bekötésben:
VDD/VDDA stabilitása és a VDD>=VDDA feltétel hardveres előfeltétel.

## 6. EXTMODE/M1 — megfelel

6-1, 19. oldal; 6-6-1 és 6-6-4: EXTMODE fixen LCD VDD=5 V.
Nem STM32 3,3 V-ra kötendő. M1 külső COM módban nem vezérel polaritást,
ezért minden parancsban következetesen 0; nincs szoftveres M1-billegtetés.

## 7. PB0/TIM3 és EXTCOMIN — megfelel a konfiguráció

ST STM32G431 DS12589 Rev 6, Table 13, 62. oldal: PB0 AF2 = TIM3_CH3;
UFQFPN48 tokon PB0 a 17-es láb, PB1 a 18-as láb (Table 12).
APB1=16 MHz, APB prescaler=1, ezért TIM3 clock=16 MHz.
PSC=15999, ARR=999: 16000000/(16000*1000)=1 Hz.
PWM2, CCR3=500: CNT 0..499 LOW, 500..999 HIGH, 50% kitöltés.
Az induló CNT=0; a HAL CC3E-t és CEN-t engedélyez. Nincs interruptos billegtetés.
A HSI16 toleranciája a tényleges frekvenciára is érvényes; műszeres mérés
flash előtt nem történt. EXTCOMIN 1 Hz mellett a belső COM frekvencia 0,5 Hz,
a Sharp 6-5-1 (28. oldal) szerint ez helyes.

További javítás: 6-6-5, 38–39. oldal szerint SCS HIGH alatti EXTCOMIN felfutás
a COM-váltást az SCS lefutásáig késlelteti. A driver ezért csak CNT=502..800
között kezd normál tranzakciót. A következő felfutásig minimum 700 ms marad,
ami a teljes kép kb. 81 ms-os pixelátviteli idejénél jelentősen hosszabb.
Az induló clear még álló timerrel megy ki. A kép megtartásakor az SCS LOW,
és nincs keepalive: ezt 6-5-3 Note 6-5-2, 30. oldal kifejezetten engedi.
Ez a rövid, megszakításmentes SPI-teszt konfigurációjára vonatkozó tartalék;
átvitel közbeni debugger-halt vagy későbbi hosszú IRQ ezt megváltoztathatja.

## 8. PA4/SCS — megfelel

PA4 kizárólag GPIO_MODE_OUTPUT_PP, nem AF5 NSS.
SPI1: NSS=SPI_NSS_SOFT, NSSPMode=DISABLE.
Aktív HIGH SCS szoftveresen vezérelt; a hardveres aktív LOW NSS nem kerül ki.

## Végleges bekötés és resetállapot

| MCU | LCD | Külső ellenállás resethez |
|---|---|---|
| PA5 | SCLK | 10 kΩ → GND |
| PA7 | SI/MOSI | 10 kΩ → GND |
| PA4 | SCS | 10 kΩ → GND |
| PB0 / AF2 TIM3_CH3 | EXTCOMIN | 10 kΩ → GND |
| PB1 | DISP | 10 kΩ → GND |
| nincs MCU-kapcsolat | EXTMODE | közvetlenül LCD VDD-re; felhúzó nem szükséges |

Az öt 10 kΩ tervezési ajánlás, nem gyári előírás. A korábbi 47 kΩ is
használható megfelelő leakage/zajviszonyok mellett, de az első hardverteszthez
a 10 kΩ határozottabb lehúzás. Nincs felhúzás az MCU által vezérelt LCD-jeleken.
LCD VDD/VDDA=5 V, VSS/VSSA=GND; STM32 saját tápja=3,3 V.

## Végső build

`scripts/build.ps1`: sikeres, 0 hiba és 0 figyelmeztetés.
Resetvektor: 0x0800072D (Thumb); kezdeti SP: 0x20005800; a linker és ST startup
változatlanul a felhasználó által azonosított STM32G431CBU6-hoz illeszkedik.
Flash 8092 bájt; RAM foglalás veremtartalékkal 2248 bájt.
A `build-st/sharp_lcd.elf`, `.hex`, `.bin` az új, auditált firmware.
FLASH READY a firmware szempontjából; a táp- és resetfeltételek hardveres
ellenőrzése szükséges. Flash, oszcilloszkópos mérés és optikai próba nem történt.
