# Build és környezetellenőrzés — 2026-11-10

Cél: a felhasználó által megadott STM32G431CBU6.
Végleges állapot: a helyi 2025-ös LD-2025315 adatlap szerinti audit után.
Részletek: `DATASHEET_AUDIT.md`.

## Végleges eredmény

- `scripts/build.ps1`: sikeres build, 0 fordítási/linkelési hiba,
  0 figyelmeztetés a végleges linken.
- Ismételt build: sikeres, `ninja: no work to do`.
- GCC: ST GNU Arm 14.3.1; Ninja: ST 1.13.2; CMake: 4.0.0.
- Saját és HAL források fordítása: `-Wall -Wextra -Werror`.
- Flash: 8092 bájt / 131072 bájt.
- RAM lefoglalt méret: 2248 bájt / 22528 bájt, ebből 2048 bájt
  veremtartalék, 4 bájt veremigazítás, 12 bájt inicializált adat és
  184 bájt BSS. A verem tényleges futásidejű maximuma nem mért.
- BIN: 8092 bájt; ELF, HEX és MAP is elkészült a `build-st/` alatt.

## Startup/linker ellenőrzés

- ST `startup_stm32g431xx.s`; `STM32G431xx` device definíció.
- Vektortábla 0x08000000.
- Kezdeti SP 0x20005800, a folytonos 22 KiB SRAM felső határa.
- Resetvektor 0x0800072D: flashben lévő Thumb belépési cím.
- `.data` flashbeli forrása 0x08001F90, SRAM célja 0x20000000.
- `.bss` 0x2000000C..0x200000C4, ST resetkezelő nullázza.
- ELF szegmensek: RX flash, RW adat, külön nulla fájlméretű RW BSS/verem.
- CCM RAM nincs bevonva; nincs tévesen folytonosként kezelt 32 KiB SRAM.

## Felmerült és javított problémák

1. A PATH régi PEAK GCC 10.2.1-et választott, a kezdetben választott
   PlatformIO Ninja 1.9.0 pedig az ismételt buildnél hibás `C/:/` dependency
   útvonallal leállt. A build-script a telepített ST GCC/Ninja eszközöket
   választja; az aktuális kimenet külön `build-st/` könyvtárba kerül.
2. Az új GCC linker a generikus CRT/newlib rendszerhívásaira és RWX
   szegmensre figyelmeztetett. A projekt a saját ST startupot használja
   `-nostartfiles` mellett, konstruktor-inicializálással és üres `_init`
   hookkal; explicit ELF szegmenseket kapott. Nem lett elnyomva warning.
3. A NOLOAD szekciók közös flash LMA-ja linkerigazítási üzenetet adott;
   külön SRAM NOLOAD szegmenssel javítva.
4. A 2025-ös adatlap végső auditja után az EXTCOMIN timer a DISP HIGH előtt
   indul; a képmegtartás alatti keepalive megszűnt (külső COM esetén nem kell).
   A normál SPI-tranzakciók csak az EXTCOMIN felfutó éle utáni biztonságos
   fázisban kezdődnek, így a COM-váltás nincs az SCS lefutásáig késleltetve.
5. A 2025-ös címtábla mind a 240 címét programozottan ellenőriztem:
   valamennyi egyezik az 1..240 címbyte hardveres LSB-first továbbításával.

## Még nem igazolt

Nem történt flash, futásidejű mérés, SPI logic-analyzer ellenőrzés vagy
optikai kijelzőteszt. Az ST-LINK csak olvasási célú lekérdezése történt:
3,31 V, STM32G43x/G44x család, 128 KiB NVM, 0x468 device ID, Rev X.
A panel kapcsolási rajza és a kijelző bekötése nem volt hozzáférhető.
Az LCD-táp sorrendjét és a reset alatti LOW jeleket a firmware önmagában
nem tudja garantálni; ehhez lásd a README hardveres feltételeit.
