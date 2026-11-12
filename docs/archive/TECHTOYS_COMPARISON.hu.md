# Techtoys driver összehasonlítás

Vizsgálat: 2026-11-11. Saját firmware: 2277ccf.
Referencia: techtoys/Arduino-Memory-LCD-Driver, 0591231aabea2be0125cf27564f55adb63af9480.
Elsődleges specifikáció: helyi Sharp LD-2025315, 2025-03-25.

## Források

- [README és tesztelt modellek](https://github.com/techtoys/Arduino-Memory-LCD-Driver/blob/0591231aabea2be0125cf27564f55adb63af9480/README.md)
- [Modellválasztás](https://github.com/techtoys/Arduino-Memory-LCD-Driver/blob/0591231aabea2be0125cf27564f55adb63af9480/src/MemoryLCD.h)
- [Driver implementáció](https://github.com/techtoys/Arduino-Memory-LCD-Driver/blob/0591231aabea2be0125cf27564f55adb63af9480/src/MemoryLCD.cpp)

## Megállapítások

| Tulajdonság | Techtoys | Saját firmware |
|---|---|---|
| LS044Q7DH01 | Kifejezetten támogatott, a README szerint tesztelt | Kizárólag ez a típus |
| Felbontás | LS044 választásakor 320x240; alapértelmezés LS027, 400x240 | 320x240 |
| SPI | Mode 0, LSB-first, 2 MHz | Mode 0, LSB-first, 1 MHz |
| SCS | GPIO, aktív HIGH | GPIO, aktív HIGH |
| Egy sor | 01 + cím + 40 adatbájt + 00 + 00 | 01/03 + cím + 40 adatbájt + 00 + 00 |
| Sorcím | 1..240, LSB-first | 1..240, LSB-first |
| Pixel | 1 fehér, 0 fekete | Ugyanaz |
| COM | Külső EXTCOMIN timer, M1 végig 0 | EXTMODE=GND, EXTCOMIN LOW, M1 vált 500 ms-onként |
| All Clear | 04 00, induláskor kétszer | 04/06 00 00, induláskor egyszer |
| Indulás | Clear kétszer, DISP HIGH, 30 us, külső COM indítás, 30 us | Tápstabilitási várakozás, Clear, 50 ms, DISP HIGH, 1 ms, periodikus M1 |

A Techtoys forrás nem tartalmaz SPI-s M1-váltás megvalósítást. Ezért az
EXTMODE=GND módunk működését ez a referencia önmagában nem igazolja.
A 01/03 és 04/06 közötti eltérés a saját kódban az aktuális M1 polaritás;
LSB-first mellett M1 valóban a 0x02 bit (Sharp nyomtatott 31., 36., 37. oldal).

## Többsoros csomag: látszólagos, de nem szerkezeti eltérés

A Techtoys UpdateBlock minden sor előtt 01-et és címet küld, az utolsó
sor után két nullát. Az első 01 módbyte. A további sorok előtti 01 az
előző sorhoz tartozó 8 dummy clock adata, nem új módparancs.
A Sharp 33. oldalán a 8 dummy + következő 8 címbit adja a 16 átviteli clockot.

A saját kód ugyanitt 00 dummy byte-ot küld, majd a következő címet.
A csomaghossz azonos: 10082 bájt, 80656 clock teljes képnél.
A dummy tartalma don't-care; a saját nullás érték megfelel az adatlapnak.
Nem indokolt ide új módbyte-ot vagy plusz dummy-t beszúrni.

## Amit nem célszerű vakon átvenni

- A Techtoys 16-clockos Clear parancsa a 2025-ös adatlap szigorú
  olvasatában rövid: 3 módclock után több mint 13 clock szerepel
  (36. oldal). A saját 24-clockos parancs ezt biztosan teljesíti.
- A kétszeri Clear a szerző tapasztalati kerülőmegoldása, nem bizonyíték
  arra, hogy a mi fehér képernyőnket az egyszeri törlés okozza. Külön,
  más változtatás nélkül érdemes majd próbálni.
- A 30 us-os indulási várakozásokat nem másoljuk át a 2025-ös specifikáció
  100 us minimumai helyett (20–22. oldal).
- Külső timeres referencia csak megfelelő EXTMODE=HIGH bekötéssel
  használható. A jelenlegi GND bekötés mellett nem helyettesíti az M1-et.

## Következtetés és korlátok

A referencia megerősíti az alapvető SPI-formátumunkat; nem találtunk
belőle bizonyított, a teljesen fehér képet megmagyarázó bitfolyamhibát.
A COM-forrás és az induláskori kétszeri törlés érdemi különbség.
A HAL_OK és az MCU-n kiolvasott GPIO-szint nem LCD-visszaigazolás.
A működés még nincs igazolva. Ez az összehasonlítás nem módosított
firmware-t, nem indított új buildet és nem flashelt.
