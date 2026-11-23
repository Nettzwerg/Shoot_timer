# Shot Timer – Einkaufsliste für Amazon

| | |
|---|---|
| **Stand** | 2026-11-23 |
| **Bezug** | [EINKAUFSLISTE.md](EINKAUFSLISTE.md) (Bauteile nach Schaltplan), [SCHALTPLAN_PROTOTYP.md](SCHALTPLAN_PROTOTYP.md) |

Diese Liste ist auf **amazon.de** zugeschnitten: Dort gibt es Elektronik meist als Mehrfachpack oder Sortiment statt einzeln. Jeder Link öffnet eine Amazon-Suche mit passenden Suchbegriffen. Das konkrete Angebot wählst du selbst. Achte auf die Spalte „Worauf achten“, dort stehen die Eigenschaften, die stimmen müssen.

> **Preise** sind Schätzwerte für typische Amazon-Packs, keine geprüften Angebote. Prime-Händler liefern in 1–2 Tagen, Angebote „Versand aus China“ brauchen oft 2–3 Wochen.

> **Gegenüber der allgemeinen Einkaufsliste geändert**, weil es diese Teile bei Amazon kaum einzeln gibt:
> - **2N7000** statt BS170 (Q3)
> - **1N5817/1N5819** aus einem Sortiment statt BAT85 (D1, D2)
>
> Beide sind für diesen Zweck gleichwertig.

---

## Warenkorb 1 – Messaufbau für den Stand (Phase 1 + 2)

| ☐ | Pos. | Artikel | Menge | Worauf achten | ca. € |
|---|---|---|---|---|---|
| ☐ | 1 | **ESP32-S3-DevKitC-1 N16R8** (16 MB Flash, 8 MB PSRAM)<br>[bei Amazon suchen](https://www.amazon.de/s?k=ESP32-S3-DevKitC-1+N16R8) | 1 (2er-Pack ideal) | **N16R8 oder N8R2**, keine N4-Variante. Board mit 2 USB-C-Buchsen („UART“ und „USB“) ist richtig. | 12–25 |
| ☐ | 2 | **DRV8833** H-Brücken-Modul<br>[bei Amazon suchen](https://www.amazon.de/s?k=DRV8833+Modul) | 1 Pack (meist 3–5 Stück) | Modul mit Pins AIN1/AIN2/AOUT1/AOUT2 und SLP/EEP. Prüfen: nSLEEP-Pull-up (Schaltplan Kap. 3.3). | 6–9 |
| ☐ | 3 | **MT3608** Step-up-Modul<br>[bei Amazon suchen](https://www.amazon.de/s?k=MT3608+Step+Up+Modul) | 1 Pack (meist 5 Stück) | Mit Trimmpoti. **Vor dem Anschließen auf 10,0 V einstellen.** | 5–8 |
| ☐ | 4 | **Elektret-Mikrofonkapseln** 9,7 mm, 2 Pins<br>[bei Amazon suchen](https://www.amazon.de/s?k=Elektret+Mikrofon+Kapsel+9%2C7mm) | 1 Pack | 2-polig. Packs mit 10–20 Stück reichen für alle Dämpfungsvarianten. | 5–8 |
| ☐ | 5 | **Piezo-Schallwandler passiv** (ohne Elektronik)<br>[bei Amazon suchen](https://www.amazon.de/s?k=Piezo+Summer+passiv) | 1–2 Packs | Nur **passiv**, nicht „aktiv“. Gut: verschiedene Größen (12 mm und 23–35 mm). Laute Typen sind oft für 12–24 V angegeben. | 6–10 |
| ☐ | 6 | **Steckbrett 830 Kontakte** + **Steckbrücken**<br>[bei Amazon suchen](https://www.amazon.de/s?k=Breadboard+830+Set+Jumper+Kabel) | 1 Set (2–3 Boards) | Sets mit 3 Breadboards und Kabeln (Stecker–Stecker, Stecker–Buchse) sind am günstigsten. | 8–12 |
| ☐ | 7 | **Widerstandssortiment** Metallschicht 1 %<br>[bei Amazon suchen](https://www.amazon.de/s?k=Widerstand+Sortiment+Metallschicht+1%25+600) | 1 | Muss enthalten: 10 Ω, 1 kΩ, 2,2 kΩ, 4,7 kΩ, 10 kΩ, 22 kΩ, 100 kΩ, 470 kΩ. | 7–10 |
| ☐ | 8 | **Keramikkondensator-Sortiment**<br>[bei Amazon suchen](https://www.amazon.de/s?k=Keramik+Kondensator+Sortiment+100nF+1uF) | 1 | Muss enthalten: 100 nF, 150 nF oder 220 nF, 1 µF. | 6–9 |
| ☐ | 9 | **Elko-Sortiment**<br>[bei Amazon suchen](https://www.amazon.de/s?k=Elektrolytkondensator+Sortiment) | 1 | 10 µF und 100 µF, mindestens 16 V. | 6–9 |
| ☐ | 10 | **Schottky-Dioden-Sortiment**<br>[bei Amazon suchen](https://www.amazon.de/s?k=Schottky+Diode+Sortiment+1N5817+1N5819) | 1 | Mit **1N5817/1N5819** – ersetzt BAT54/BAT85 (D1, D2) und SS14 (D3). | 5–8 |
| ☐ | 11 | **N-MOSFET 2N7000** (TO-92)<br>[bei Amazon suchen](https://www.amazon.de/s?k=2N7000+MOSFET) | 1 Pack | Ersetzt 2N7002/BS170 (Q3). Gleichwertig für diesen Zweck. | 5–7 |
| ☐ | 12 | **P-MOSFET AO3401** (SOT-23)<br>[bei Amazon suchen](https://www.amazon.de/s?k=AO3401+SOT-23) | 1 Pack | SMD, braucht Adapter (Pos. 13). | 4–7 |
| ☐ | 13 | **Adapterplatinen SOT-23 / SOT-89 → DIP**<br>[bei Amazon suchen](https://www.amazon.de/s?k=SOT23+SOT89+Adapter+Platine+DIP) | 1 Set | Zum Auflöten von AO3401 und HT7833, dann steckbrettfähig. | 6–9 |
| ☐ | 14 | **Abgeschirmtes Mikrofonkabel** 2-adrig<br>[bei Amazon suchen](https://www.amazon.de/s?k=Mikrofonkabel+2+adrig+abgeschirmt) | 2 m | Alternativ: 3,5-mm-Klinken-Verlängerung zerschneiden. | 4–6 |
| ☐ | 15 | **Stiftleisten** 2,54 mm<br>[bei Amazon suchen](https://www.amazon.de/s?k=Stiftleiste+2%2C54mm+40+polig) | 1 Pack | Gerade, zum Anlöten an Module und Adapter. | 4–6 |
| ☐ | 16 | **USB-C-Kabel, datenfähig**<br>[bei Amazon suchen](https://www.amazon.de/s?k=USB+C+Kabel+Daten) | 1 | Kein reines Ladekabel, sonst klappt das Flashen nicht. | 5–8 |

**Summe Warenkorb 1:** ca. **95–150 €**. Das ist mehr als die Bauteilkosten, weil Sortimente, Mehrfachpacks und Reserveteile dabei sind. Diese reichen auch für Phase 3 und spätere Projekte.

### Gibt es bei Amazon selten – besser bei Reichelt oder Berrybase

| ☐ | Artikel | Menge | Hinweis | ca. € |
|---|---|---|---|---|
| ☐ | **MCP6004-I/P** (DIP-14) | 2 | Bauform **DIP-14 / PDIP**, nicht SOIC. Bei Reichelt Bestellnummer „MCP 6004-I/P“. | 1,50 |
| ☐ | IC-Fassung 14-polig | 2 | | 0,30 |
| ☐ | **HT7833** (SOT-89) – erst für Phase 3 | 2 | 3,3-V-Regler mit wenig Ruhestrom. **Nicht durch AMS1117 ersetzen**, dessen Spannungsabfall ist für einen LiPo zu groß. | 1 |

Am einfachsten bestellst du diese drei Teile zusammen bei Reichelt. Dort kommt noch Versand dazu (Mindermengenzuschlag beachten).

---

## Warenkorb 2 – Funktionsmuster mit Display und Akku (Phase 3, später)

| ☐ | Pos. | Artikel | Menge | Worauf achten | ca. € |
|---|---|---|---|---|---|
| ☐ | 17 | **2,0"-IPS-Display ST7789, 240×320, SPI**<br>[bei Amazon suchen](https://www.amazon.de/s?k=2+inch+LCD+ST7789+240x320+SPI) | 1 | Anschlüsse VCC, GND, SCL/CLK, SDA/DIN, RES/RST, DC, CS, BLK/BL. Markenmodul (z. B. Waveshare 2inch LCD) oder günstiges Generikmodul. | 8–16 |
| ☐ | 18 | **TP4056-Lademodul USB-C mit Schutz**<br>[bei Amazon suchen](https://www.amazon.de/s?k=TP4056+Type+C+Lademodul+Schutz) | 1 Pack (meist 5–10 Stück) | **Mit Schutzschaltung** (6 Anschlüsse: IN+, IN−, B+, B−, OUT+, OUT−). | 5–8 |
| ☐ | 19 | **LiPo-Akku 3,7 V 2000 mAh, Bauform 103450**, mit JST-PH-2.0<br>[bei Amazon suchen](https://www.amazon.de/s?k=LiPo+3.7V+2000mAh+103450+JST) | 1 | Mit Schutzplatine. **Polarität am Stecker prüfen**, sie ist bei Akkus nicht einheitlich. | 9–14 |
| ☐ | 20 | **JST-PH-2.0-Buchsen 2-polig**<br>[bei Amazon suchen](https://www.amazon.de/s?k=JST+PH+2.0+2+Pin+Buchse+Set) | 1 Set | Passend zum Akku; Sets mit Kabel sind praktisch. | 6–8 |
| ☐ | 21 | **Drucktaster 16 mm**, nicht rastend (START)<br>[bei Amazon suchen](https://www.amazon.de/s?k=Drucktaster+16mm+tastend) | 1 Pack | Tastend (Momentschalter), **nicht** rastend. | 6–9 |
| ☐ | 22 | **Taster 12 × 12 mm mit Kappen**<br>[bei Amazon suchen](https://www.amazon.de/s?k=Taster+12x12+mit+Kappen) | 1 Set | Sets mit 25 Tastern und Kappen. | 6–8 |
| ☐ | 23 | **Schiebeschalter** Platinenmontage<br>[bei Amazon suchen](https://www.amazon.de/s?k=Schiebeschalter+SS12D00) | 1 Pack | SS12D00 oder ähnlich, 2,54 mm Raster. | 4–6 |
| ☐ | 24 | **Lochrasterplatinen**<br>[bei Amazon suchen](https://www.amazon.de/s?k=Lochrasterplatine+doppelseitig+Set) | 1 Set | Doppelseitig, verschiedene Größen; 7 × 9 cm wird gebraucht. | 6–9 |

**Summe Warenkorb 2:** ca. **50–80 €** (wieder mit Mehrfachpacks).

---

## Werkzeug (falls nicht vorhanden)

| ☐ | Werkzeug | Suche | Wofür |
|---|---|---|---|
| ☐ | **Multimeter** | [bei Amazon suchen](https://www.amazon.de/s?k=Digital+Multimeter) | **Pflicht:** MT3608 auf 10 V einstellen, Inbetriebnahme |
| ☐ | **Lötstation** (temperaturgeregelt) + Lötzinn | [bei Amazon suchen](https://www.amazon.de/s?k=L%C3%B6tstation+temperaturgeregelt) | Stiftleisten, Adapter, Leitungen |
| ☐ | **Seitenschneider** und **Abisolierzange** | [bei Amazon suchen](https://www.amazon.de/s?k=Abisolierzange+Seitenschneider+Set) |  |
| ☐ | **Gehörschutz** | [bei Amazon suchen](https://www.amazon.de/s?k=Geh%C3%B6rschutz+Kapsel) | beim Beep-Test auf voller Lautstärke |
| ☐ | Schallpegelmesser (optional) | [bei Amazon suchen](https://www.amazon.de/s?k=Schallpegelmesser) | alternativ Smartphone-App |

---

## Schon vorhanden oder ohne Kauf

- **Powerbank** für den Stand. Vorher 15 min testen, ob sie bei geringer Last eingeschaltet bleibt (Schaltplan Kap. 6.1).
- **Box** für den Messaufbau, z. B. eine Frischhaltedose.
- **Offenporiger Schaumstoff**, z. B. aus einer Verpackung (Dämpfungsvariante B).
- **PETG-Filament** für Mikrofonhalter und Kappen (ca. 20 g).
- **10-kΩ-Widerstand von GPIO 4 nach GND**, solange kein Akku angeschlossen ist. Er ist im Widerstandssortiment enthalten.
