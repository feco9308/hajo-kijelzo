# STM32G431CBU6 + Sharp LS044Q7DH01 hardverteszt

**Aktuális hibakeresési mód:** 2 s teljes fehér / 2 s teljes fekete ismétlődik,
folyamatosan HIGH DISP mellett (`SHARP_SIMPLE_BLINK=ON`). A fázisok és
értelmezésük a `DIAGNOSTIC.md` fájlban találhatók.
A lent leírt egyszer lefutó alapteszthez `SHARP_SIMPLE_BLINK=OFF` és
`SHARP_DIAGNOSTIC_LOOP=OFF` szükséges.
USB-s naplózás is elkészült és COM10-en ellenőrzött: használat és a mezők
magyarázata a `USB_DIAGNOSTICS.md` fájlban, olvasó script: `scripts/monitor.ps1`.
**Aktuális jelminőségi próba:** SPI1 250 kHz; a lent leírt 1 MHz az eredeti
beállítás. A többi LCD-jelbeállítás változatlan.

Minimális C11 / STM32 HAL projekt VS Code-hoz. Saját kijelződriver, soronként
40 bájt pixeladattal; nincs framebuffer, grafikus könyvtár vagy dinamikus memória.

## Ellenőrzött célhardver

- A felhasználó által leolvasott MCU: **STM32G431CBU6**, UFQFPN48, 128 KiB flash.
- ST-LINK lekérdezés: STM32G43x/G44x, device ID 0x468, Rev X, 128 KiB NVM,
  3,31 V céltáp. Ez családot és flashméretet igazol, a teljes tokozáskódot nem.
- ST hivatalos `startup_stm32g431xx.s` és `system_stm32g4xx.c` fájlok.
- Linker: flash 0x08000000, 128 KiB; folytonos SRAM 0x20000000, 22 KiB;
  kezdeti SP 0x20005800. A különálló 10 KiB CCM SRAM nincs használatban.
- Belső HSI16 órajel, PLL és külső kvarc nélkül. SPI1: 16 MHz / 16 = névleg 1 MHz.
  A tényleges frekvenciát a HSI tűrése befolyásolja.

| MCU láb | Kijelzőjel | Működés |
|---|---|---|
| PA5 | SCLK | AF5 SPI1, mode 0 |
| PA7 | SI | AF5 SPI1 MOSI, LSB first |
| PA4 | SCS | GPIO, **aktív HIGH** |
| PB0 | EXTCOMIN | AF2 TIM3_CH3, hardveres 1 Hz, 50% kitöltés |
| PB1 | DISP | GPIO, induláskor LOW |

Az LCD VDD és VDDA 5 V, EXTMODE fixen VDD; VSS és VSSA közös GND.
**Az MCU saját tápja 3,3 V marad.** A panel pontos típusa és kapcsolási rajza
nem ismert: ellenőrizd, hogy nincs más áramkör ezekre a lábakra kötve.

## Indítás és teszt

1. GPIO kimeneti latch-ek LOW-ra állítása, majd kimeneti mód engedélyezése.
2. HSI16, SPI1 és TIM3 konfigurálása. EXTCOMIN ekkor még LOW.
3. 100 ms várakozás a már bekapcsolt LCD-tápra.
4. All Clear, majd 50 ms biztonsági várakozás (TA minimum 100 µs).
5. EXTCOMIN hardveres PWM indítása LOW fázissal, 1 ms várakozás, DISP HIGH.
6. Az első EXTCOMIN felfutó él után legalább 2 ms várakozás, majd teljes fehér kép 3 másodpercig.
7. Teljes fekete kép 3 másodpercig.
8. 16×16 pixeles sakktábla, utána ezen marad.

A kép megtartása alatt nem küldünk SPI-keepalive-ot: a 2025-ös adatlap
6-5-3 Note 6-5-2 szerint ilyenkor az SCS LOW maradhat. Minden normál
tranzakció a timer CNT=502..800 tartományában kezdődik, így a következő
EXTCOMIN felfutó élig legalább 700 ms marad a kb. 81 ms-os pixelátvitelre.
Ez elkerüli az SCS HIGH alatti felfutó él miatt késleltetett COM-váltást.
A TIM3
megszakítás és főprogrambeli GPIO-billegtetés nélkül működik. Debug halt alatt
nincs TIM3 freeze; MCU reset vagy tápvesztés viszont leállítja a jelet.
Hibánál DISP LOW, SPI-jelek LOW; a már futó timer tovább jár.
A debuggerben `test_stage`: 1=inicializálás, 2=fehér, 3=fekete, 4=sakktábla,
0xFFFFFFFF=hiba.

## Driver API

`Core/Inc/sharp_ls044q7dh01.h`, `Core/Src/sharp_ls044q7dh01.c`:

```c
HAL_StatusTypeDef sharp_init(void);
HAL_StatusTypeDef sharp_clear(void);
HAL_StatusTypeDef sharp_fill(bool white);
HAL_StatusTypeDef sharp_write_line(uint16_t line, const uint8_t *data);
HAL_StatusTypeDef sharp_test_pattern(void);
HAL_StatusTypeDef sharp_keepalive(void);
```

A `line` **1..240**; `data` pontosan 40 olvasható bájt. Bit0 az adott bájt
első pixele, 1=fehér, 0=fekete. Az API blokkoló, főprogramból hívható, nem
reentráns. Minden visszatérési értéket ellenőrizni kell. A `sharp_init()` a
panel perifériáinak inicializálása után hívható, és egyszer indítja a timert.
Egy API-hívás kb. 702 ms-ot is várhat a megfelelő timerfázisra.

Parancsok LSB-first perifériával: write=0x01, clear=0x04, display=0x00.
A clear/display csomag 24 órajelet ad; ez teljesíti a 3 mód-bit utáni
13-nál több átviteli órajel követelményét. Egyetlen sor végén 16 dummy órajel
van. Teljes képnél egy SCS HIGH alatt több sor kerül ki: soronként 8 dummy
bit és a következő sor címe, az utolsó sor után további 8 dummy bit.
SCS setup legalább 4 µs, hold legalább 2 µs. A HAL megvárja a SPI BSY végét.
Csomagok között PA5 és PA7 GPIO LOW módba kerül, PA4 is LOW.

## Hardveres feltételek az első flash előtt

- A Sharp specifikáció 2,7 V minimum HIGH szintet ír elő, így a 3,3 V-os
  jelek megfelelőek. A LOW maximum 0,15 V: rövid vezeték, jó közös föld és
  tényleges jelszintmérés szükséges a megbízható próbához.
- Reset alatt az MCU lábai nagy impedanciájúak. A biztos LOW induláshoz
  külső, egyenként **10 kΩ GND-re húzás ajánlott** SCS, SCLK, SI, DISP és
  EXTCOMIN vonalakon. Ez tervezési ajánlás; az adatlap nem ír elő ellenállásértéket.
  Ezt a firmware önmagában nem tudja garantálni.
- A program nem kapcsolja és nem méri az LCD tápját. VDD/VDDA legyen stabil,
  mielőtt jelet hajtunk rá; VDD a VDDA-val együtt vagy előbb emelkedjen.
  Ne legyen HIGH vezérlőjel táp nélküli kijelzőn. A tápok összehangolása
  hardveres feladat ezzel a bekötéssel.
- A kijelző közelében VDD és VDDA áganként 1 µF kerámia puffer javasolt.
- Kikapcsolás előtt All Clear, memória-inicializálási várakozás, DISP LOW,
  majd a jelek LOW-ra állítása szükséges; ez a demonstráció nem vezérel
  táp-lekapcsolási folyamatot. A 5 V-ot ne kapcsold le működő MCU-jelek mellett.
- Debuggerrel lehetőleg SCS LOW nyugalmi ponton állj meg; SPI-átvitel közbeni
  halt felboríthatja a COM időzítését, mert TIM3 tovább fut.
- Hosszú debugger-reset vagy megszakadó EXTCOMIN mellett ne hagyd a panelt
  tartósan aktív állapotban. Hosszan tartó statikus képhez külön frissítési
  és képmegfordítási stratégia szükséges; ez rövid hardverteszt.

## Build VS Code-ból vagy PowerShellből

VS Code: **Ctrl+Shift+B**, vagy a projekt gyökeréből:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

A script előnyben részesíti a telepített STM32 bundle GCC/Ninja eszközeit.
Az ellenőrzött környezet: CMake 4.0.0, ST GCC 14.3.1, ST Ninja 1.13.2,
STM32CubeProgrammer 2.22.0. Szükséges HAL/CMSIS források helyben vannak.
A build nem igényel hálózatot vagy CubeMX-et.

Kimenetek: `build-st/sharp_lcd.elf`, `.hex`, `.bin`, `.map`,
`compile_commands.json`. **A korábbi `build/` mappa csak az első, régi
eszközökkel végzett próbafordítást tartalmazza; a `build-st/` az aktuális.**

## Flash ST-LINK-kel

SWDIO→PA13, SWCLK→PA14, GND→GND, céltáp referencia→3,3 V; NRST bekötése
ajánlott és a script under-reset csatlakozásához szükséges lehet.
A célpanelt megfelelő saját tápról üzemeltesd.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\flash.ps1
```

Ez a művelet felülírja a panel jelenlegi firmware-ét. A script a
CubeProgrammerrel SWD 1 MHz-en csatlakozik, letölti az ELF-et, ellenőrzi,
majd resetel. VS Code-ban a `Flash ST-LINK (replaces firmware)` task ugyanez.
CubeProgrammer GUI-val is megnyitható a HEX, majd Download / Verify / Reset.
Ha az under-reset kapcsolódás sikertelen, először NRST-t és a céltápot ellenőrizd.

## Első műszeres ellenőrzés

SCS nyugalomban LOW, átvitelkor HIGH; SCLK kb. 1 MHz, idle LOW. Dekódolás:
SPI mode 0, LSB first. Kezdeti clear csomag: `04 00 00`.
EXTCOMIN PB0: névleg 500 ms LOW / 500 ms HIGH. DISP a clear, a várakozás és
a timer elindítása után HIGH.
Ha nincs kép, ezeket és a tápokat mérd meg először. A sikeres build nem igazolja
a bekötés vagy a kijelző működését; automatikus flash és optikai próba nem történt.

## Források és rögzített verziók

- **Elsődleges referencia:** a projektgyökérben található
  `SPEC_LS044Q7DH01_LD2025315_20250325.pdf`, Sharp LD-2025315, 2025-03-25,
  6-1, 6-2, 6-3, 6-5, 6-6 és 6-8 fejezet. A teljes audit: `DATASHEET_AUDIT.md`.
- A korábbi implementációhoz használt 2011-es LCP2111038A helyett a végleges
  audit és indítási/COM-időzítés a helyi 2025-ös gyári specifikációra támaszkodik.
- [ST STM32G431 adatlap, DS12589](https://www.st.com/resource/en/datasheet/stm32g431cb.pdf).
- [ST HAL G4](https://github.com/STMicroelectronics/stm32g4xx-hal-driver),
  v1.2.7, commit `a6001282dfacfff57e9710250f15e4333b578865`.
- [ST CMSIS Device G4](https://github.com/STMicroelectronics/cmsis-device-g4),
  commit `626ee412334a5ed2e5b320af5a8d77d69f03a558`.
- [ARM CMSIS 5.9.0](https://github.com/ARM-software/CMSIS_5/tree/5.9.0),
  commit `2b7495b8535bdcb306dac29b9ded4cfb679d7e5c`; csak Core/Include checkout.

A külső források saját licencfájljai a Drivers alatti könyvtárakban találhatók.
