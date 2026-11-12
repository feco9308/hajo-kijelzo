# ST-LINK demóadatok és parancsok

A normál belépési pont `scripts/send-demo.ps1`. A script a
`scripts/stlink-command.ps1` közös SWD-kezelését használja. A példákat és
értéktartományokat a projekt README-je tartalmazza.

Az ST-LINK SWDIO, SWCLK, GND és célfeszültség-referencia kapcsolattal
csatlakozik, NRST a feltöltéshez ajánlott. Az MCU és az LCD tápellátása
szükséges. Az adatküldés HOTPLUG kapcsolatot használ.

## Adatformátum

A `lcd_debug` RAM-struktúra első négy 32 bites mezője request, result,
completed, last_command. A 16. byte-tól nyolc 32 bites adatmező következik.

| Index | C-mező | Egység / típus |
|---:|---|---|
| 0 | awa_degrees | Előjeles fok, int32 |
| 1 | aws_tenths | Tized csomó, uint32 |
| 2 | tws_tenths | Tized csomó, uint32 |
| 3 | sog_tenths | Tized csomó, uint32 |
| 4 | cog_degrees | Fok, uint32 |
| 5 | xte_hundredths | Század tengeri mérföld, int32 |
| 6 | distance_tenths | Tized tengeri mérföld, uint32 |
| 7 | waypoint | 1…99, uint32 |

A PowerShell script az alkalmazott adatokat olvassa, az explicit megadott
mezőket módosítja, ellenőrzi és beírja a teljes adatcsomagot. A visszaellenőrzött
csomag után írja be a request=11 parancsot. Az MCU pillanatképet vesz,
validál és újrarajzol. Sikeres képírás után tárolja az új állapotot,
visszamásolja az adatokat a mailboxba, növeli a completed számlálót,
végül törli a request mezőt.

A host a completed és last_command alapján ellenőriz, majd az alkalmazott
adatokat is visszaolvassa. Result=0 siker, 1…3 HAL-státusz, 4 érvénytelen
adat. Hibás bemenetnél az alkalmazott adatok és a kép megmaradnak.
SPI-hibánál a korábbi alkalmazott adatok megmaradnak és a DISP LOW-ra vált.

Egy Windows névvel ellátott mutex sorosítja a scriptből kiadott SWD-kéréseket,
beleértve az olvasás–módosítás–írás teljes folyamatát. A firmware a képet
főprogramból rajzolja, a COM timer ettől függetlenül fut.

A címek a `build-st/sharp_lcd.elf` szimbólumtáblájából származnak. Az ELF
egyezzen a feltöltött firmware-rel. A régi négymezős mailbox parancsai
megtartották a helyüket, a demóadatok a struktúra végére kerültek.

## Parancskódok

1 clear, 2 white, 3 black, 4 pattern, 5 invert, 6 on, 7 off, 8 stop,
9 checkerboard, 10 boat, 11 demóadatok alkalmazása.

A CubeProgrammer egy gyorsan elfogyasztott request írásánál visszaolvasási
hibát is jelezhet. A script ilyenkor a completion számlálót ellenőrzi.
Egy kérést egyszer küld el.

A demóadatok RAM-ban élnek. Újraindításkor a beépített mintaértékek
állnak vissza. A képernyőn végig DEMO jelölés látszik.
