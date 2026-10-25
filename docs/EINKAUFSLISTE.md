# Shot Timer – Einkaufsliste

| | |
|---|---|
| **Stand** | 2026-10-25 |
| **Bezug** | [SCHALTPLAN_PROTOTYP.md](SCHALTPLAN_PROTOTYP.md) (Referenzbezeichnungen R1, Q1 … in Klammern) |

Die Liste ist nach Bauphasen gegliedert. **Teil 1 reicht für den Messtermin am Stand** (Phase 1 und 2). Teil 2 wird erst für das Funktionsmuster mit Akku und Display gebraucht und kann später bestellt werden.

Preise sind grobe Schätzwerte für den Einzelbezug. Bei AliExpress ist es meist günstiger, die Lieferung dauert aber 1–3 Wochen. Amazon, Reichelt, Berrybase oder Conrad liefern schneller, sind aber teurer.

> **Bedrahtet statt SMD:** Wo im Schaltplan ein SMD-Bauteil steht, ist hier, wenn möglich, ein bedrahteter Ersatz für das Steckbrett genannt. Er ist elektrisch gleichwertig. Für die wenigen reinen SMD-Teile gibt es Adapterplatinen (Pos. 1.9).

---

## Teil 1 – Messaufbau für den Stand (Phase 1 + 2)

### 1.1 Hauptkomponenten

| ☐ | Pos. | Bauteil | Menge | Suchbegriff / Hinweis | ca. € |
|---|---|---|---|---|---|
| ☐ | 1.1 | **ESP32-S3-DevKitC-1** mit **mindestens 8 MB Flash** (N8R2, N8R8 oder N16R8) | 1 (+1 Reserve empfohlen) | „ESP32-S3-DevKitC-1 N16R8“. **Keine N4-Variante**: Die Mess-Firmware braucht 8 MB Flash für die Speicherung der Messdaten. | 7–10 |
| ☐ | 1.2 | **MCP6004-I/P** (4 × OPV, DIP-14) (U1) | 2 (1 Reserve) | Bauform **DIP-14 / PDIP** auswählen, nicht SOIC | 1,50 |
| ☐ | 1.3 | IC-Fassung 14-polig | 2 | | 0,30 |
| ☐ | 1.4 | **Elektret-Kapsel Ø 9,7 mm**, 2 Anschlüsse (MIC) | 5 | „Elektret Mikrofonkapsel 9,7 mm“; möglichst unterschiedliche Typen bzw. Empfindlichkeiten (−42 bis −46 dB) | 2 |
| ☐ | 1.5 | **DRV8833-Modul** (H-Brücke) | 2 (1 Reserve) | „DRV8833 Motortreiber Modul 2 Kanal“ | 3 |
| ☐ | 1.6 | **MT3608-Modul** (Boostwandler) | 2 (1 Reserve) | „MT3608 Step Up Modul“ | 2 |
| ☐ | 1.7 | **Piezo-Signalgeber, passiv** (ohne eingebaute Elektronik) | 3 verschiedene | „Piezo Schallwandler passiv 3 kHz“, „Piezo Buzzer passiv 12 V“, Piezo-Scheibe 35 mm mit Gehäuse. **Nicht** „aktiv“ bzw. „mit Oszillator“ | 5 |

### 1.2 Halbleiter und Kleinteile

| ☐ | Pos. | Bauteil | Menge | Ersetzt im Schaltplan | ca. € |
|---|---|---|---|---|---|
| ☐ | 1.8 | **AO3401** P-MOSFET (SOT-23) | 5 | Q1, Q2 (Q4 in Teil 2) | 1 |
| ☐ | 1.9 | **Adapterplatinen SOT-23 → DIP** (und SOT-89 für Teil 2) | 1 Set | für AO3401 und HT7833 | 2 |
| ☐ | 1.10 | **BS170** N-MOSFET (TO-92) | 3 | 2N7002 (Q3) | 0,60 |
| ☐ | 1.11 | **BAT85** Schottky-Diode (bedrahtet) | 5 | BAT54 (D1, D2) | 0,50 |

### 1.3 Widerstände und Kondensatoren

Wer noch keine Sortimente hat, kauft am günstigsten je ein Sortiment. Die Sortimente kosten einmalig etwas mehr, reichen aber für alle Phasen und spätere Projekte.

| ☐ | Pos. | Bauteil | benötigte Werte | ca. € |
|---|---|---|---|---|
| ☐ | 1.12 | **Widerstandssortiment** Metallschicht 1 %, ¼ W (E12, ca. 600 Stück) | 10 Ω, 1 kΩ, 2,2 kΩ, 4,7 kΩ, 10 kΩ, 22 kΩ, 100 kΩ (+ 470 kΩ für Teil 2) | 6–8 |
| ☐ | 1.13 | **Keramikkondensator-Sortiment** | 100 nF (ca. 6 Stück), **150 nF** (2 Stück; 220 nF geht auch → Hochpass ca. 70 Hz), 1 µF (1 Stück) | 5 |
| ☐ | 1.14 | **Elko-Sortiment** (16–25 V) | 10 µF (3 Stück), 100 µF (2 Stück) | 4 |

### 1.4 Aufbau, Leitung, Stromversorgung

| ☐ | Pos. | Bauteil | Menge | Hinweis | ca. € |
|---|---|---|---|---|---|
| ☐ | 1.15 | Steckbrett 830 Kontakte | 2 | eins für Analogteil, eins für Beep-Stufe | 5 |
| ☐ | 1.16 | Steckbrücken-Set (Stecker–Stecker, Stecker–Buchse) | 1 | | 3 |
| ☐ | 1.17 | **Abgeschirmte Leitung 2-adrig**, ca. 2 m | 1 | Mikrofonleitung; alternativ eine 3,5-mm-Klinkenverlängerung zerschneiden | 2 |
| ☐ | 1.18 | Stiftleisten (gerade) | 1 Streifen | zum Anlöten an Module/Adapter | 1 |
| ☐ | 1.19 | **USB-Kabel, datenfähig**, passend zum DevKit (USB-C oder Micro-USB, je nach Board) | 1 | reine Ladekabel funktionieren nicht zum Flashen | 3 |
| ☐ | 1.20 | **Powerbank** | 1 | für den Stand; vorhandene vorher 15 min testen (Kap. 6.1 im Schaltplan-Dokument), ggf. mit Kleingeräte-/Low-Current-Modus | (vorhanden) |
| ☐ | 1.21 | Box für den Messaufbau (z. B. Frischhaltedose ca. 20 × 15 cm) | 1 | Steckbretter + Powerbank, Loch für die Mikrofonleitung | 2 |
| ☐ | 1.22 | Offenporiger Schaumstoff | etwas | Dämpfungsvariante B; Verpackungsschaum oder Mikrofon-Windschutz | – |
| ☐ | 1.23 | Filament PETG für Mikrofonhalter und Kappen | ca. 20 g | | – |

**Zwischensumme Teil 1:** ca. **30–35 €** ohne Sortimente, ca. **45–55 €** mit Sortimenten und Reserveteilen.

---

## Teil 2 – Funktionsmuster (Phase 3), später bestellen

| ☐ | Pos. | Bauteil | Menge | Suchbegriff / Hinweis | ca. € |
|---|---|---|---|---|---|
| ☐ | 2.1 | **IPS-TFT 2,0", ST7789, 240×320, SPI** | 1 | „2.0 inch IPS TFT ST7789 240x320 SPI“; mit Anschluss für die Hintergrundbeleuchtung (BLK) | 5 |
| ☐ | 2.2 | **TP4056-Lademodul USB-C mit Schutzschaltung** | 2 (1 Reserve) | „TP4056 Type-C mit Schutz“ (Modul mit DW01 und 8205A, Anschlüsse B+/B− und OUT+/OUT−) | 1,50 |
| ☐ | 2.3 | **LiPo-Akku 1S 2000 mAh, Bauform 103450**, mit Schutzplatine und JST-PH-2.0-Stecker | 1 | ca. 50 × 34 × 10 mm; wegen Versandbeschränkungen am besten bei einem Händler in Deutschland bestellen | 7–10 |
| ☐ | 2.4 | JST-PH-2.0-Buchse 2-polig (passend zum Akku) | 2 | Polarität vor dem Einstecken prüfen, sie ist bei Akkus nicht einheitlich | 0,50 |
| ☐ | 2.5 | **HT7833** Spannungsregler 3,3 V, SOT-89 | 2 | auf Adapterplatine aus Pos. 1.9 | 1 |
| ☐ | 2.6 | **1N5817** Schottky-Diode (bedrahtet) | 2 | ersetzt SS14 (D3) | 0,30 |
| ☐ | 2.7 | Taster für Frontplatte Ø 16 mm, nicht rastend (START) | 1 (+1) | „Drucktaster 16 mm Taster kurzhubig“ | 2 |
| ☐ | 2.8 | Taster 12 × 12 mm mit Kappen | 4 (+2) | „Taster 12x12 mit Kappe“ | 1 |
| ☐ | 2.9 | Schiebeschalter (Akku trennen) | 1 | Platinenmontage, 2 Positionen | 0,30 |
| ☐ | 2.10 | Lochrasterplatine ca. 7 × 9 cm | 2 | | 2 |
| ☐ | 2.11 | Gürtelclip | 1 | gedruckt oder gekauft | 1 |
| ☐ | 2.12 | Polycarbonat- oder Acrylscheibe 1 mm (Displayschutz) | 1 | | 1 |
| ☐ | 2.13 | Schrauben M2/M2,5 + Einschmelzmuttern | etwas | Gehäuse | 2 |

**Zwischensumme Teil 2:** ca. **25 €**

---

## Werkzeug und Messmittel (falls nicht vorhanden)

| ☐ | Werkzeug | Wofür | Pflicht? |
|---|---|---|---|
| ☐ | **Multimeter** | **MT3608 vor dem Anschließen auf 10,0 V einstellen**, Spannungen bei der Inbetriebnahme prüfen | **ja** |
| ☐ | Lötkolben (temperaturgeregelt) + Lötzinn | Stiftleisten, Adapterplatinen, Mikrofonleitung | ja |
| ☐ | Seitenschneider, Abisolierzange | | ja |
| ☐ | Schallpegelmesser | Lautstärke des Beeps (Phase 2); alternativ Smartphone-App (ungenauer) | nein |
| ☐ | 3D-Drucker (oder Druckdienst) | Mikrofonhalter, Kappen, später Gehäuse | für Phase 1 empfohlen |
| ☐ | Gehörschutz | beim Testen des Beeps auf voller Lautstärke | ja |

---

## Software (kostenlos)

- [VS Code](https://code.visualstudio.com/) mit der Erweiterung **PlatformIO IDE** (Firmware flashen)
- [Python 3](https://www.python.org/) mit `pip install pyserial matplotlib` (Auslesen und Auswerten)
- Ggf. Treiber für den USB-Seriell-Wandler des DevKits (CP210x oder CH343, je nach Board)
