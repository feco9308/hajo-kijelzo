# Hajókijelző

STM32G431CBU6 alapú vitorlás műszerfal Sharp LS044Q7DH01 320×240
monokróm Memory LCD kijelzőhöz. A fejlesztés egyetlen ága a `main`.
Induláskor a szél- és navigációs műszerfal jelenik meg DEMO jelöléssel.

## Helye a gépen

A projekt mappája `C:\Users\User\Kijelzo`. VS Code-ban ezt a mappát kell
megnyitni. A feltölthető program `build-st\sharp_lcd.elf`, a nyers kép
`build-st\sharp_lcd.bin`. Minden további módosítás ehhez a projekthez tartozik.

| Feladat | Fájl |
|---|---|
| Műszerek, betűk, elrendezés, demóadatok | `Core/Src/boat_demo.c` |
| Adatok típusa | `Core/Inc/boat_demo.h` |
| Sharp protokoll | `Core/Src/sharp_ls044q7dh01.c` |
| STM32 inicializálás, SPI és timer | `Core/Src/main.c` |
| ST-LINK parancsfogadó | `Core/Src/debug_control.c` |
| Demóadat-küldés a számítógépről | `scripts/send-demo.ps1` |

## Demóadatok ST-LINK-en

Az STM32 és az LCD legyen bekapcsolva, az ST-LINK csatlakozzon SWD-n.
A számítógépen az ST STM32CubeProgrammer és STM32 GNU toolchain szükséges.
A PowerShell-parancsokat a projekt mappájában kell futtatni.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/send-demo.ps1 -Awa -45 -Aws 18.2 -Tws 13.4 -Sog 6.8 -Cog 268 -Xte -0.05 -Distance 1.7 -Waypoint 2
```

Egyetlen érték külön is frissíthető. A többi értéket a program megőrzi.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/send-demo.ps1 -Aws 21.4
```

Paraméter nélkül az aktuális adatokat olvassa vissza.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/send-demo.ps1
```

| Paraméter | Jelentés | Tartomány |
|---|---|---|
| Awa | Látszólagos szélirány, fok, pozitív a jobb oldal | −180…180, egész |
| Aws | Látszólagos szélsebesség, csomó | 0…99,9 |
| Tws | Valós szélsebesség, csomó | 0…99,9 |
| Sog | Föld feletti sebesség, csomó | 0…99,9 |
| Cog | Föld feletti haladási irány, fok | 0…359, egész |
| Xte | Előjeles oldaleltérés, tengeri mérföld | −9,99…9,99 |
| Distance | Waypoint távolsága, tengeri mérföld | 0…99,9 |
| Waypoint | Waypoint száma | 1…99, egész |

A sebességek és távolság tizedre, az XTE századra kerekítve kerülnek át.
A parancsokban tizedespont használható. A mutató követi az AWA értékét.
A DEMO felirat minden képen megmarad. Az adatokat most a számítógép adja.
Újraindításkor a program a beépített kezdő demóértékeket tölti be.

## Build és feltöltés

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/flash.ps1
```

VS Code-ban a Ctrl+Shift+B ugyanezt a buildet indítja. A kész képfájl
mindig a `build-st` mappába kerül. Az ST-LINK parancsscript a hozzá tartozó
ELF szimbólumtáblájából olvassa a RAM-címeket, ezért a helyi build és a
feltöltött firmware verziójának egyeznie kell.

Friss klónozás után a rögzített ST/ARM függőségek telepítése
`powershell -NoProfile -ExecutionPolicy Bypass -File scripts/bootstrap.ps1`.

## Bekötés az LCD gyári számozása szerint

| LCD láb | Jel | Bekötés |
|---:|---|---|
| 1 | SCLK | PA5, SPI1 AF5 |
| 2 | SI / MOSI | PA7, SPI1 AF5 |
| 3 | SCS | PA4 GPIO, aktív HIGH |
| 4 | EXTCOMIN | PB0, AF2 TIM3_CH3, 1 Hz, 50% |
| 5 | DISP | PB1 GPIO |
| 6 | VDDA | 5 V |
| 7 | VDD | 5 V |
| 8 | EXTMODE | VDD / 5 V |
| 9 | VSS | Közös GND |
| 10 | VSSA | Közös GND |

Az LCD 1-es végét a saját szalagcsatlakozóján kell azonosítani.
A megfordított 1…10 sorrend okozta a korábbi működési hibát.
Az STM32 és LCD földje közös. Az STM32 kimenetek 3,3 V-osak.
Reset idejére a vezérlőjeleken kb. 10 kΩ külső lehúzás ajánlott.
Az EXTMODE bekötését tápmentesen lehet változtatni.

SPI1 normál 2LINES master, mode 0, LSB-first, 250 kHz. A sorokat külön
SCS-tranzakciók írják. A külső COM timer képtartás alatt is fut.

## Ellenőrzés és diagnosztika

A műszerfalat ST-LINK-kel is vissza lehet kérni.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/stlink-command.ps1 -Command boat
```

Ugyanebben a firmware-ben a `checkerboard`, `white`, `black`, `on`, `off`
és `status` diagnosztikai parancsok is használhatók. Az `off` után `on`
kapcsolja vissza a kijelzést. Az USB CDC konzolon a `boat` / `vitorlas`
paranccsal a műszerfal rajzolható újra.

A demóadatok ellenőrzése valós ST-LINK kapcsolaton fut, és a végén
visszaállítja a teszt előtti adatokat.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test-stlink-data.ps1
```

A vizsgálat előjeles és részleges frissítést, határértékeket, firmware-oldali
elutasítást és két párhuzamos küldést is ellenőriz. HAL_OK az STM32 oldali
végrehajtás eredménye, a kijelző külön visszaigazolást nem ad.

A korábbi kísérletek feljegyzései a `docs/archive` mappában találhatók.
A projektnek egy CMake firmware célja és egy aktív `main` ága van.
