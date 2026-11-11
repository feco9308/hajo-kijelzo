# LCD vezérlése ST-LINK/SWD kapcsolaton

A jelenlegi 250 kHz-es, soronkénti SPI-próba változatlan maradt.
Az új RAM-parancsfogadó USB-kapcsolat nélkül is használható, az MCU saját
USB-kábele elhagyható. Az MCU-nak és az LCD-nek ettől még megfelelő táp kell.
ST-LINK: SWDIO, SWCLK, GND, célpanel feszültségreferencia; NRST ajánlott.

PowerShell a projekt gyökerében:

```powershell
.\scripts\stlink-command.ps1 -Command on
.\scripts\stlink-command.ps1 -Command black
.\scripts\stlink-command.ps1 -Command pattern
.\scripts\stlink-command.ps1 -Command status
```

További parancsok: clear, white, invert, off, stop. A képírások nem
kapcsolják be automatikusan a DISP-et; off után előbb on szükséges.
A script nem resetel vagy állítja meg az MCU-t, HOTPLUG kapcsolatot használ.
A hardveres EXTCOMIN közben folyamatosan fut.

## Megvalósítás és korlátok

A `lcd_debug` globális struktúra sorrendben négy 32 bites mező:
request, result, completed, last_command. A debugger csak a request mezőt
írja. Kódok: 1 clear, 2 white, 3 black, 4 pattern, 5 invert, 6 on,
7 off, 8 stop. A főprogram végzi az SPI-műveletet, nem debuggerből
hívunk HAL-függvényt és nem írunk közvetlenül GPIO-regisztert.
Minden kérés megállítja az automata képírást. Egyszerre egy kérés engedélyezett.
USB-ről és SWD-ről ne adjunk egymással versengő parancsokat.

A script a build-st/sharp_lcd.elf szimbólumtáblájából olvassa a RAM-címet;
az ELF-nek meg kell egyeznie a feltöltött firmware-rel. Futó CPU szükséges.
A completed számláló és a last_command bizonyítja a végrehajtást, nem a
RAM-ba írás sikerüzenete. CubeProgrammer esetenként ellenőrzési hibát
jelezhet, ha a program már feldolgozta és törölte a request mezőt az
eszköz visszaolvasása előtt. A script ilyenkor is a completion mezőket
ellenőrzi; nem küldi újra a kérést.

A `status` itt a RAM-parancsfogadó állapotát adja vissza, nem az USB-s
részletes állapotsort. HAL=0 továbbra is csak MCU-oldali siker: nincs
LCD-visszaigazolás vagy kijelzőkép-visszaolvasás.

## Ellenőrzés

Hibamentes build, ST-LINK feltöltés és flash-visszaellenőrzés.
Valós hardveren SWD-n végrehajtva: on, off, clear, white, black,
pattern, invert, stop; completion számláló növekedett, minden HAL=0.
A vizsgálat végén on + black állapot maradt. A látható kép helyességét
ez nem igazolja; ehhez felhasználói megfigyelés szükséges.
