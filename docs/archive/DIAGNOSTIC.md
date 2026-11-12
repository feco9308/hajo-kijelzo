# Aktív, ismétlődő diagnosztika

**Jelenlegi javító branch:** USB alapból OFF, SPI 2-line, állandó PA5/PA7 AF5,
fekete/fehér teszt. Részletek: `MINIMAL_BRINGUP.md`. Az alábbiak részben a
korábbi USB-s diagnosztikát írják le.

**Aktuális próba:** `SHARP_SIMPLE_BLINK=ON` felülírja a lent részletezett
ötfázisú diagnosztikát. Az inicializálás után DISP végig HIGH, a teljes
kijelző 2 s fehér (stage=12), majd 2 s fekete (stage=13), folyamatosan.
Az átviteli idő és timerfázisra várakozás miatt a látható váltás 2–3 s lehet.
Nincs szándékos DISP LOW vagy több egymást követő fehér tesztfázis.
USB CDC és EXTCOMIN konfiguráció változatlan.
**Jelminőségi próba:** SPI1 most 250 kHz (HSI16 / 64), az eredeti 1 MHz helyett.
A teljes képernyő tiszta átviteli ideje kb. 323 ms; a COM-időzítési ablakban
legalább 700 ms marad a következő felfutásig. A 2025-ös adatlap nem ad meg
SCLK minimumot. A lassítás teszt, nem igazolt hibajavítás.
A soros napló `spi_hz=250000` mezővel jelzi az aktuális névleges frekvenciát.
Az ötfázisú mód visszaállítása: `SHARP_SIMPLE_BLINK=OFF`,
`SHARP_DIAGNOSTIC_LOOP=ON`. Az eredeti egyszer lefutó módhoz mindkettő OFF.

2026-11-10: a vékony csík / hibás kép elkülönítésére a
`SHARP_DIAGNOSTIC_LOOP` CMake opció alapértelmezésben ON.
Az eredeti egyszer lefutó teszt megmaradt az OFF konfigurációban.
A SPI frekvenciája, bitsorrendje és a driver protokollja változatlan.

Az indulási inicializálás után a következő fázisok ismétlődnek:

| test_stage | 3 másodperces fázis | Elvárt eredmény |
|---|---|---|
| 10 | DISP LOW, nincs SPI-forgalom | teljes panel fehér / kikapcsolt kijelzés |
| 11 | All Clear, majd DISP HIGH | teljes panel fehér |
| 12 | teljes képernyő fehér adatírással | teljes panel fehér |
| 13 | teljes képernyő fekete adatírással | teljes panel fekete |
| 14 | sakktábla adatírással | teljes panel 16x16 sakktábla |

A ciklus kb. 15–20 másodperc, a SPI-idő és a timerfázisra várakozás miatt.
EXTCOMIN mindvégig tovább fut. Hiba esetén stage=0xFFFFFFFF, DISP LOW.

Ha a stage 10 alatt is csak egy csík reagál, először az LCD csatlakozóján
kell mérni a tápokat, közös földet és DISP-jelet. Ha ott teljes panel-fehér
van, de az adatok hibásak, SCS/SCLK/SI bekötés és jelalak vizsgálandó.
Ezek elkülönítési lépések, nem bizonyítják önmagukban egy alkatrész hibáját.

Az előző DATASHEET_AUDIT.md/BUILD_REPORT.md méretadatai az auditált,
egyszer lefutó változathoz tartoznak; az aktuális diagnosztikai build eltér.
