# USB CDC diagnosztika — WeAct Studio STM32G4 Core Board

**A `fix/minimal-spi-bringup` branchen a USB alapból ki van kapcsolva.**
Ezért nem jelenik meg COM-port. Az itt leírt funkció `SHARP_USB_DIAG=ON`
konfigurációval állítható vissza; a driverpróba alatt ST-LINK a diagnosztika.

**Aktuális kijelzőteszt:** `SHARP_SIMPLE_BLINK=ON`; csak stage 12 (fehér)
és stage 13 (fekete) ismétlődik, egyenként 2 s várakozással. DISP végig HIGH.
A lent felsorolt stage 10/11/14 az elérhető ötfázisú teszt jelentése.
A lenti méretadatok a korábbi USB-s ötfázisú firmware-re vonatkoznak.
Aktuális jelminőségi próba: SPI1 250 kHz, `spi_hz=250000` naplómezővel.
Az eredeti 1 MHz csak az összehasonlítás alapja; a lassítás eredményét a
kijelző megfigyelése és szükség esetén műszeres jelalakvizsgálat dönti el.

MCU: STM32G431CBU6. A felhasználó megerősítette a panel típusát.
A WeAct QFN48 gyári kapcsolási rajzán az USB-C D-/D+ PA11/PA12-re csatlakozik;
az LCD öt vezérlőlábával nincs ütközés. Nem szükséges külön USB-UART adapter.

Az USB CDC eszköz-funkció a firmware része, az ST hivatalos USB Device
middleware CDC osztályával, HAL PCD-vel. Windows alatt már sikeresen megjelent
a virtuális soros port és olvasható volt a napló: külön Windows driver
telepítésére nem volt szükség.

## Használat

2026-11-10-én az észlelt port **COM10**. Az USB kihúzása/átdugása után
a Windows másik számot is rendelhet hozzá.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\monitor.ps1 -Port COM10 -Seconds 20
```

Folyamatos figyeléshez `-Seconds 0`, leállítás Ctrl+C. A script kilépéskor
bezárja a portot. Terminálprogram is használható: 115200, 8N1, flow control
nélkül, DTR engedélyezve. A baud rate USB CDC vonalkódolási beállítás;
nem a fizikai USB sebessége. Egyszerre csak egy program nyissa meg a COM-portot.

Példa valóban visszaolvasott sor:

```text
LS044_DIAG ms=40948 stage=13 disp=1 extcom=1 tim3=796 cs=0 hal=0 spi_err=0 reset_flags=469762048
```

| Mező | Jelentés |
|---|---|
| ms | MCU uptime milliszekundumban |
| stage | 10=DISP LOW, 11=All Clear, 12=fehér adatírás, 13=fekete, 14=sakktábla |
| disp | PB1 MCU bemeneti visszaolvasása |
| extcom | PB0 MCU bemeneti visszaolvasása |
| tim3 | TIM3 számlálója, 0..999 |
| cs | PA4 MCU bemeneti visszaolvasása; az idle naplóban 0 |
| hal | utolsó ellenőrzött HAL eredmény: 0=OK, 1=ERROR, 2=BUSY, 3=TIMEOUT |
| spi_err | HAL SPI error bitmask; 0=nincs jelzett hiba |
| reset_flags | induláskor mentett RCC_CSR reset bitek, decimális alakban |

Másodpercenként egy sor készül. Az 1 Hz-es EXTCOMIN és a másodpercenkénti
mintavétel miatt az extcom/tim3 pillanatértékei ismétlődhetnek: ez önmagában
nem jelenti a timer leállását. A napló nem oszcilloszkópos frekvenciamérés.
A bitek az MCU lábain mértek, nem az LCD-csatlakozón. A kijelző SI bemenete
egyirányú; nincs visszaigazolása arról, hogy fogadta-e a képet.

## Megvalósítás

- USB: full speed, HSI48 48 MHz; CRS automatikus trim USB SOF-ról.
- SYSCLK/SPI/TIM3 továbbra is HSI16-ról működik; a korábbi frekvenciák megmaradtak.
- A PA11/12 a G431 USB additional-function lába; nincs kitalált AF-szám.
- PMA: endpoint tábla 0x00..0x3F; EP0 OUT=0x40, EP0 IN=0x80,
  CDC IN=0xC0, CDC OUT=0x100, CDC CMD=0x140, egymást nem fedik.
- Statikus CDC memória és állandó TX/RX bufferek; nincs heap vagy printf.
- Nem blokkoló küldés, host nélkül nincs várakozás USB-re.
- Az USB-kezelés nem módosítja az LCD protokollját. USB IRQ-k közben a TIM3
  hardveres PWM változatlanul fut. Suspend nem lépteti STOP módba az MCU-t.
- RX adatot a firmware eldobja, nincsenek vezérlőparancsok.
- Sikeres USB inicializálás: `diag_usb_status=HAL_OK`, debuggerből is olvasható.

## Ellenőrzés

ST GCC 14.3.1, CMake 4.0.0, Ninja 1.13.2: sikeres teljes build, 0 hiba,
0 warning, `-Wall -Wextra -Werror`. Flash 23236 bájt, SRAM 4968 bájt a
2 KiB veremtartalékkal. A végleges, valóban feltöltött ELF/HEX/BIN a `build-st/` alatt.
ST-LINK download + verify + reset sikeres.
Windows enumeráció és COM10 naplóolvasás sikeres; fázisváltások és
`hal=0`, `spi_err=0` megfigyelve. A port bezárása/újranyitása is tesztelt.
Az LCD képhibáját az USB elkészítése nem oldotta meg és nem igazolta annak okát.

## Elsődleges források

- [WeAct gyári panelrepo és kapcsolási rajz](https://github.com/WeActStudio/WeActStudio.STM32G431CoreBoard),
  Hardware/QFN48/WeAct-STM32G431CxUxCoreBoard_V10_SchDoc.pdf.
- [ST USB Device middleware](https://github.com/STMicroelectronics/stm32-mw-usb-device),
  rögzített commit: `2df324bd60d4b0bb27404fd70b1c089b467f0e09`.
- [ST CubeG4 CDC példa](https://github.com/STMicroelectronics/STM32CubeG4/tree/master/Projects/STM32G474E-EVAL/Applications/USB_Device/CDC_Standalone).
  A descriptor c/h innen származik, az eszköznév módosítva.
  A források szerzői jogi megjegyzései megmaradtak; licencek:
  USB/ST_EXAMPLE_LICENSE.md és Drivers/STM32_USB_Device_Library/LICENSE.md.

A korábbi audit/buildjelentés méretadatai a korábbi, USB nélküli változatra vonatkoznak.
