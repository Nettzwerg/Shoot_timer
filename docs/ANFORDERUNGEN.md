# Shot Timer auf ESP32-Basis – Anforderungsspezifikation

| | |
|---|---|
| **Dokument** | Lastenheft / Anforderungsspezifikation |
| **Version** | 0.2 (Entwurf) – Schwerpunkt IPSC-Training in Halle und am Stand |
| **Datum** | 2026-10-25 |
| **Status** | Zur Abstimmung |

---

## 1. Zweck und Ziel

Es soll ein eigener **Shot Timer** (Schusszeitmessgerät) für das sportliche Schießen entwickelt werden. Das Gerät gibt ein akustisches Startsignal, erkennt Schüsse über ein Mikrofon und misst die Zeiten vom Startsignal bis zu jedem Schuss.

Ziele:

1. **Funktionsumfang** vergleichbar mit Geräten am Markt (z. B. CED7000, AMG Lab Commander, PACT Club Timer, Special Pie M1A2).
2. **Basis ESP32**, Bedienung autark über **Display und Tasten** (keine App nötig).
3. **Deutlich günstiger** als kommerzielle Geräte: Ziel-Materialkosten (BOM) **≤ 40 €**, harte Obergrenze **60 €** (Marktgeräte liegen etwa bei 100–250 €).
4. Nachbaubar mit gängigen Bauteilen und Hobby-Werkzeug (Lötkolben, 3D-Drucker).

### 1.1 Abgrenzung (nicht im Umfang von Version 1)

- Keine Trefferauswertung an der Scheibe (kein elektronisches Trefferanzeigesystem).
- Keine Zulassung als offizielles Wettkampf-Zeitmessgerät eines Verbands.
- Keine Eingriffe an Waffen; das Gerät arbeitet ausschließlich akustisch.

### 1.2 Priorisierung

Anforderungen sind nach **MoSCoW** priorisiert:

- **M** = Muss (Version 1)
- **S** = Soll (Version 1, falls machbar)
- **K** = Kann (spätere Version)

---

## 2. Begriffe

| Begriff | Bedeutung |
|---|---|
| **String / Serie** | Ein Durchgang vom Startsignal bis zum Stoppen des Timers. |
| **Startverzögerung (Delay)** | Zeit zwischen Druck auf START und dem Startsignal (Beep). |
| **Erster Schuss (First Shot / Draw)** | Zeit vom Beep bis zum ersten erkannten Schuss. |
| **Split** | Zeit zwischen zwei aufeinanderfolgenden Schüssen. |
| **Gesamtzeit (Total)** | Zeit vom Beep bis zum letzten erkannten Schuss. |
| **Par-Zeit** | Vorgegebene Zielzeit, bei deren Ablauf ein zweiter Beep ertönt. |
| **Sensitivität** | Schwelle, ab der ein Geräusch als Schuss gilt. |
| **Echo-/Totzeit** | Zeit nach einem erkannten Schuss, in der keine weiteren Schüsse gezählt werden (unterdrückt Echos/Nachhall). |
| **Spy-/Listen-Modus** | Timer zeichnet Schüsse eines anderen Schützen auf, ohne selbst ein Startsignal zu geben. |
| **Hit Factor** | Punkte ÷ Zeit (IPSC/USPSA-Wertung). |

---

## 3. Funktionale Anforderungen

### 3.1 Zeitmessung (Kern)

| ID | Anforderung | Prio |
|---|---|---|
| F-001 | Nach Druck auf START ertönt nach der eingestellten Startverzögerung ein Startsignal; ab dem Beginn des Signals läuft die Zeitmessung. | M |
| F-002 | Jeder erkannte Schuss wird mit seiner Zeit relativ zum Startsignal gespeichert. | M |
| F-003 | Auflösung der Anzeige: **0,01 s**. Interne Auflösung: ≤ 1 ms. | M |
| F-004 | Mindestens **99 Schüsse pro String**. | M |
| F-005 | Maximale Messdauer pro String mindestens **999,99 s**. | M |
| F-006 | Während des Strings live angezeigt: letzte Schusszeit, Schussnummer, Split zum vorherigen Schuss. | M |
| F-007 | Der String endet durch Tastendruck, nach einstellbarer Ruhezeit ohne Schuss (Auto-Stop, z. B. 5–60 s oder aus) oder bei Erreichen der maximalen Messdauer. | M |
| F-008 | Ein neuer Druck auf START beginnt einen neuen String. | M |

### 3.2 Startsignal und Startverzögerung

| ID | Anforderung | Prio |
|---|---|---|
| F-010 | Startverzögerung **zufällig** in einstellbarem Bereich (Standard 1,0–4,0 s, wählbar etwa 0,5–10 s). | M |
| F-011 | Startverzögerung **fest**, einstellbar von 0,0 bis 10,0 s in 0,1-s-Schritten. | M |
| F-012 | Startverzögerung **sofort** (Instant, 0 s). | M |
| F-013 | Tonhöhe des Beeps einstellbar (mindestens 3 Stufen, Bereich ca. 1–4 kHz). | S |
| F-014 | Beepdauer einstellbar (z. B. 0,1–1,0 s). | S |
| F-015 | Lautstärke des Beeps einstellbar (mindestens 3 Stufen). | S |
| F-016 | Der eigene Beep darf nicht als Schuss erkannt werden (Unterdrückung während des Beeps). | M |
| F-017 | Optionaler Countdown im Display während der festen Verzögerung. | K |

### 3.3 Par-Zeiten

| ID | Anforderung | Prio |
|---|---|---|
| F-020 | Mindestens **1 Par-Zeit** einstellbar (0,1–99,99 s); bei Ablauf ertönt ein unterscheidbarer Beep. | M |
| F-021 | Bis zu **10 Par-Zeiten** in Folge (für Drills mit mehreren Zeitabschnitten). | S |
| F-022 | Par-Zeiten können ein-/ausgeschaltet werden, ohne die Werte zu verlieren. | M |
| F-023 | Schüsse nach Ablauf der Par-Zeit werden in der Auswertung markiert. | S |

### 3.4 Schusserkennung

| ID | Anforderung | Prio |
|---|---|---|
| F-030 | Akustische Schusserkennung über eingebautes Mikrofon. | M |
| F-031 | Sensitivität in mindestens **8 Stufen** einstellbar. | M |
| F-032 | Echo-/Totzeit einstellbar (etwa 0,02–0,50 s, Standard 0,05 s). | M |
| F-033 | Splits bis hinunter zu **0,06 s** müssen sicher getrennt erkannt werden (schnelle Schützen erreichen ca. 0,10 s). | M |
| F-034 | Genauigkeit der erkannten Schusszeit: **±10 ms** gegenüber dem tatsächlichen Schall am Mikrofon. | M |
| F-035 | Zuverlässige Erkennung für Großkaliber-Kurzwaffen (9 mm, .45) indoor und outdoor. | M |
| F-036 | Erkennung für Kleinkaliber (.22 lfB) und Luftdruck (10 m) bei hoher Sensitivität. Für Luftdruck ist ein Zusatzsensor zulässig (siehe F-037). | S |
| F-037 | Anschluss eines externen Sensors (z. B. Klinkenbuchse für Ansteck-Mikrofon oder Erschütterungssensor/Piezo an der Waffe) für Luftdruck und Trockentraining. | K |
| F-038 | Hilfe zur Einstellung: Anzeige des aktuellen Pegels als Balken („Mikrofon-Test“), damit der Schütze die Sensitivität kalibrieren kann. | M |
| F-039 | Fehldetektionen durch Schützen auf Nachbarbahnen reduzieren: Auto-Kalibrierung der Schwelle anhand des Umgebungspegels, Anzeige des Spitzenpegels je Schuss im Review, Prüfung der Anstiegszeit (siehe Hardware-Konzept, Kap. 4). | M |

### 3.5 Auswertung (Review)

| ID | Anforderung | Prio |
|---|---|---|
| F-040 | Nach dem String: Anzeige von **Schussanzahl, erstem Schuss, Gesamtzeit, schnellstem und langsamstem Split, Durchschnitts-Split**. | M |
| F-041 | Durchblättern aller Schüsse eines Strings mit Nummer, Zeit und Split. | M |
| F-042 | Löschen des letzten Schusses (falls fehlerhaft erkannt), mit Neuberechnung aller Werte. | S |
| F-043 | Anzeige der letzten Schüsse während des Strings (z. B. die letzten 3). | S |

### 3.6 Speicher und Historie

| ID | Anforderung | Prio |
|---|---|---|
| F-050 | Mindestens **100 Strings** werden nichtflüchtig gespeichert (bleiben beim Ausschalten erhalten). | M |
| F-051 | Durchblättern der gespeicherten Strings mit Datum/Uhrzeit (sofern Uhrzeit bekannt) bzw. laufender Nummer. | M |
| F-052 | Löschen einzelner oder aller Strings. | M |
| F-053 | Bei vollem Speicher werden die ältesten Strings überschrieben (Ringpuffer), mit Hinweis. | S |
| F-054 | Einstellungen bleiben beim Ausschalten erhalten. | M |

### 3.7 Betriebsmodi

| ID | Modus | Beschreibung | Prio |
|---|---|---|---|
| F-060 | **Standard (Timer)** | Startverzögerung → Beep → Schüsse messen → Review. | M |
| F-061 | **Par-Modus** | Wie Standard, zusätzlich Par-Beep(s). | M |
| F-062 | **Spy/Listen** | Kein Startsignal; die Messung beginnt mit Tastendruck oder mit dem ersten erkannten Geräusch; erfasst Splits fremder Schützen. | M |
| F-063 | **Trockentraining (Dry Fire)** | Nur Start- und Par-Beep, keine Schusserkennung. Optional wiederholend (Auto-Repeat mit einstellbarer Pause). | M |
| F-064 | **Wiederholung (Repetitive)** | Nach Ablauf/Ende startet automatisch nach einstellbarer Pause der nächste Durchgang. | S |
| F-065 | **Countdown** | Feste Zeitvorgabe, Ende-Beep, optional Warn-Beeps (z. B. bei 10 s und 5 s); z. B. für Zeitserien. | S |
| F-066 | **Duell / Rapid-Fire** (ISSF/DSB Sportpistole, Duellteil) | Zyklus: Pause (Standard 7 s) – Beep – Schusszeit (Standard 3 s) – Ende-Beep; Anzahl Zyklen einstellbar (Standard 5). | K |
| F-067 | **Zeitserien** (z. B. 25 m Schnellfeuer: 8 s / 6 s / 4 s) | Vordefinierte Abfolgen von Vorbereitungs- und Schusszeiten als auswählbare Presets. | K |
| F-068 | **Hit Factor / Comstock** | Nach dem String Eingabe von Punkten (A/C/D/Miss/No-Shoot) und Berechnung des Hit Factors. | S |
| F-069 | **Benutzerdefinierte Drills** | Speichern von mindestens 10 Presets (Name, Delay, Par-Zeiten, Modus). | S |

### 3.8 Bedienung und Menü

| ID | Anforderung | Prio |
|---|---|---|
| F-070 | Vollständige Bedienung über Tasten am Gerät, ohne Smartphone. | M |
| F-071 | Große, auch mit Handschuhen gut tastbare **START-Taste**, getrennt von den Menütasten. | M |
| F-072 | Menütasten: mindestens **AUF, AB, OK/MENÜ, ZURÜCK** (alternativ Drehencoder mit Drucktaste). | M |
| F-073 | Wichtigste Funktionen in höchstens **2 Tastendrücken** erreichbar (Modus wechseln, Review, Sensitivität). | S |
| F-074 | Langer Druck auf START = Ein/Aus. Langer Druck auf ZURÜCK = zurück zum Hauptbildschirm. | S |
| F-075 | Menüsprache Deutsch; Englisch umschaltbar. | S / K |
| F-076 | Tastensperre gegen versehentliches Verstellen. | K |

Vorgeschlagene Menüstruktur:

```
Hauptbildschirm (Modus, letzte Zeit, Akku)
├── Modus:       Standard | Par | Spy | Dry Fire | Repetitive | Countdown | Duell | Presets
├── Start:       Verzögerung (Zufall/Fest/Sofort), Bereich, Beep (Ton/Dauer/Lautstärke)
├── Par-Zeiten:  Anzahl, Par 1…10, Ein/Aus
├── Erkennung:   Sensitivität, Echo-Zeit, Mikrofon-Test, Auto-Stop
├── Review:      Letzter String, Historie, Löschen
├── Presets:     Laden, Speichern, Umbenennen
└── System:      Helligkeit, Auto-Aus, Sprache, Uhrzeit, Bluetooth/WLAN, Werkseinstellung, Info/Version
```

### 3.9 Konnektivität (ESP32-Vorteil)

| ID | Anforderung | Prio |
|---|---|---|
| F-080 | **Export** der gespeicherten Strings als CSV (über USB-seriell oder WLAN). | S |
| F-081 | **BLE-Schnittstelle**, die Schusszeiten live an ein Smartphone sendet (eigene Web-App oder einfaches GATT-Profil). | K |
| F-082 | **Weboberfläche** im Access-Point-Modus zum Einstellen, Ansehen und Herunterladen der Strings. | K |
| F-083 | **Firmware-Update** über USB, optional OTA über WLAN. | M (USB) / S (OTA) |
| F-084 | Funk ist standardmäßig aus (Energie sparen) und wird bei Bedarf eingeschaltet. | S |
| F-085 | Kompatibilität mit vorhandenen Apps (z. B. Protokolle kommerzieller Timer) wird **nicht** angestrebt. | – |

---

## 4. Nicht-funktionale Anforderungen

### 4.1 Akustik und Signal

| ID | Anforderung | Prio |
|---|---|---|
| N-001 | Startsignal mindestens **100 dB(A) in 30 cm** (gut hörbar unter Gehörschutz, auch outdoor). Ziel: 105–110 dB. | M |
| N-002 | Frequenz des Beeps im Bereich 2–4 kHz (dort ist der Piezo am lautesten und das Signal unter Gehörschutz gut wahrnehmbar). | M |
| N-003 | Mikrofon und Eingangsstufe übersteuern bei Schusspegeln (> 150 dB SPL am Schützen) nicht dauerhaft, sondern erholen sich innerhalb der Echo-Zeit. | M |

### 4.2 Display

| ID | Anforderung | Prio |
|---|---|---|
| N-010 | Zeitwert in großer Schrift (mindestens 10 mm Ziffernhöhe), aus ca. 1 m lesbar. | M |
| N-011 | **Ablesbar in direktem Sonnenlicht** und in dunklen Indoor-Ständen (Beleuchtung bzw. Helligkeit einstellbar). | M |
| N-012 | Displaydiagonale 1,3"–2,4". | M |

### 4.3 Energieversorgung

| ID | Anforderung | Prio |
|---|---|---|
| N-020 | Akkubetrieb mit eingebautem Li-Ion-/LiPo-Akku. | M |
| N-021 | Laden über **USB-C** (5 V), Laden während des Betriebs möglich. | M |
| N-022 | Laufzeit mindestens **8 Stunden** aktiver Trainingsbetrieb (≈ 1 Trainingstag), Ziel 20 h. | M |
| N-023 | Akkuanzeige im Display (mindestens 4 Stufen) und Warnung bei niedrigem Ladestand. | M |
| N-024 | Automatisches Abschalten nach einstellbarer Inaktivität (z. B. 5–60 min). | M |
| N-025 | Schutz gegen Tiefentladung, Überladung und Kurzschluss (Akku mit Schutzschaltung). | M |
| N-026 | Standby-/Aus-Stromaufnahme so gering, dass der Akku mehrere Monate nicht tiefentladen wird (Ziel < 50 µA). | S |

### 4.4 Mechanik und Umwelt

| ID | Anforderung | Prio |
|---|---|---|
| N-030 | Gehäuse handlich, Zielmaß ca. **100 × 60 × 30 mm**, Gewicht < 200 g. | S |
| N-031 | Gürtelclip bzw. Befestigung für Gürtel und Taschenausschnitt; optional Magnet oder 1/4"-Stativgewinde. | S |
| N-032 | Gehäuse 3D-druckbar (PETG/ASA), robust gegen Fall aus 1 m auf harten Boden. | M |
| N-033 | Spritzwassergeschützt (Ziel etwa IP54), Mikrofon- und Lautsprecheröffnung mit Membran bzw. Gewebe. | S |
| N-034 | Betriebstemperatur **−10 °C bis +45 °C** (Laden: 0 bis 40 °C). | S |
| N-035 | Mikrofonöffnung so platziert, dass sie beim Tragen am Gürtel nicht verdeckt wird. | M |

### 4.5 Software / Firmware

| ID | Anforderung | Prio |
|---|---|---|
| N-040 | Firmware für ESP32 in C/C++ (Arduino-Framework oder ESP-IDF, Build mit PlatformIO). | M |
| N-041 | Zeitkritische Teile (Schusserkennung, Zeitstempel, Beep) laufen unabhängig von Display und Menü (eigene Task bzw. Interrupt, hohe Priorität). | M |
| N-042 | Startzeit nach dem Einschalten < 2 s bis zur Betriebsbereitschaft. | S |
| N-043 | Modularer Aufbau (Erkennung, Signal, UI, Speicher, Konnektivität getrennt), damit neue Modi leicht ergänzt werden können. | M |
| N-044 | Quellcode und Hardware-Dateien versioniert im Repository (Open Source optional). | S |
| N-045 | Testmodus zur Selbstprüfung (Beep, Tasten, Display, Mikrofonpegel, Akku). | S |

### 4.6 Kosten

| ID | Anforderung | Prio |
|---|---|---|
| N-050 | Materialkosten (Elektronik, Akku, Gehäusematerial) **≤ 40 €** bei Einzelstück-Bezug über gängige Händler; maximal 60 €. | M |
| N-051 | Nur gut verfügbare Standardbauteile bzw. fertige Module; keine Fine-Pitch-Bauteile, die Reflow-Löten zwingend erfordern (Variante 1: Module auf Lochraster oder einfacher Platine). | M |
| N-052 | Optional spätere eigene Platine (z. B. bei JLCPCB/PCBWay gefertigt) zur Kostensenkung und Verkleinerung. | K |

---

## 5. Hardware-Anforderungen (vorläufiges Konzept)

> Diese Angaben sind ein **Lösungsvorschlag** zur Abschätzung von Machbarkeit und Kosten. Die endgültige Auswahl erfolgt im Hardware-Konzept (nächster Schritt).

| ID | Baugruppe | Anforderung / Vorschlag | ca. Kosten |
|---|---|---|---|
| H-010 | **Mikrocontroller** | ESP32-S3 oder ESP32 (WROOM-Modul bzw. Dev-Board), ≥ 4 MB Flash; ADC oder I²S für Mikrofon, BLE/WLAN integriert. | 4–8 € |
| H-020 | **Mikrofon** | Variante A: Elektret-Kapsel + Verstärker mit Hüllkurven-/Spitzenwertdetektor + Komparator auf einen Interrupt-Pin (sehr exakter Zeitstempel, robust gegen Übersteuerung). Variante B: digitales I²S-MEMS-Mikrofon mit hohem maximalem Schalldruck (AOP ≥ 120 dB) und Auswertung per Software. Sensitivität über Software-Schwelle, ggf. digitales Poti. Auswahl nach Labortest am Schießstand. | 1–5 € |
| H-030 | **Signalgeber** | Piezo-Signalgeber (passiv, Resonanz ca. 2,7–4 kHz), angesteuert über H-Brücke oder Treiber mit erhöhter Spannung (z. B. 12 V aus Boost-Wandler) für ≥ 100 dB. Alternativ kleiner Lautsprecher + Klasse-D-Verstärker (z. B. MAX98357A). | 2–5 € |
| H-040 | **Display** | Sonnenlichttauglich: transflektives/reflektives LCD (z. B. Sharp Memory LCD) **oder** IPS-TFT (ST7789, 1,9–2,0") mit hoher Helligkeit. OLED (SSD1306/SH1106) nur als günstige Prototyp-Variante, da outdoor schlecht ablesbar. | 3–15 € |
| H-050 | **Tasten** | 1 große START-Taste (Ø ≥ 12 mm) + 4 Menütasten, entprellt; alternativ Drehencoder. Mindestens eine Taste kann den ESP32 aus dem Deep-Sleep wecken. | 1–3 € |
| H-060 | ~~**Ausgang (optional)**~~ | Entfällt (Entscheidung v0.2). | – |
| H-070 | **Stromversorgung** | 1S-LiPo bzw. 18650 (≥ 1500 mAh) mit Schutzschaltung, USB-C-Laderegler (z. B. TP4056 mit Schutz oder IP5306/MCP73831), LDO mit geringem Ruhestrom (z. B. HT7333/ME6211), Akkuspannungsmessung per Spannungsteiler (abschaltbar). | 4–8 € |
| H-080 | **Ein/Aus** | Softpower (Latch-Schaltung) oder Deep-Sleep mit Wake-on-Button; optional mechanischer Hauptschalter zur Lagerung. | 0,5–1 € |
| H-090 | **Uhrzeit** | Optional RTC (z. B. DS3231/PCF8563) oder Zeit über WLAN/NTP bzw. App für Zeitstempel der Strings. | 0–3 € |
| H-100 | **Externer Sensor (optional)** | 3,5-mm-Klinke für externes Mikrofon oder Piezo-Erschütterungssensor (Luftdruck, Trockentraining). | 0,5–1 € |
| H-110 | **Gehäuse** | 3D-Druck, Kleinteile (Schrauben, Gewebe für Öffnungen, Gürtelclip). | 3–5 € |
| | | **Summe (Schätzung)** | **ca. 20–55 €** |

---

## 6. Sicherheit und rechtliche Hinweise

| ID | Anforderung | Prio |
|---|---|---|
| S-001 | Gehörschutzhinweis: Der Beep ist bei voller Lautstärke nahe am Ohr gesundheitsschädlich → Hinweis in der Anleitung, Lautstärke beim ersten Einschalten auf mittlere Stufe. | M |
| S-002 | Li-Ion-Akku nur mit Schutzschaltung; Ladeelektronik mit Temperatur- bzw. Strombegrenzung; Akku mechanisch gegen Quetschen geschützt. | M |
| S-003 | Das Gerät beeinflusst keine Waffenfunktion und wird nicht an der Waffe befestigt (Ausnahme: optionaler externer Sensor, rein passiv und klemmend, ohne Veränderung der Waffe). | M |
| S-004 | Funkbetrieb nur im 2,4-GHz-Band mit dem zertifizierten ESP32-Modul (Modul mit CE-Konformität, keine eigenen HF-Schaltungen). | M |
| S-005 | Beachtung der Schießstandordnung: Der Timer ersetzt keine Kommandos der Standaufsicht. | M |

---

## 7. Abnahmekriterien (Auszug)

| Nr. | Test | Kriterium |
|---|---|---|
| A-01 | Zeitgenauigkeit | Vergleich mit Referenz (Oszilloskop bzw. zweites Mikrofon + Audioaufnahme): Abweichung ≤ ±10 ms über 50 Schüsse. |
| A-02 | Schnelle Splits | Simulierte Impulse (Lautsprecher, Klatschen, Platzpatrone) mit 0,08 s Abstand: 100 % getrennt erkannt bei Echo-Zeit 0,05 s. |
| A-03 | Schießstand outdoor | 9-mm-Pistole, 3 Strings à 10 Schuss: keine fehlende und höchstens 1 zusätzliche Detektion bei korrekter Sensitivität. |
| A-04 | Schießstand indoor | Wie A-03, mit Schützen auf den Nachbarbahnen; Ergebnis dokumentieren, Einstellhinweise ableiten. |
| A-05 | Lautstärke | ≥ 100 dB(A) in 30 cm (Schallpegelmesser bzw. kalibrierte App). |
| A-06 | Laufzeit | ≥ 8 h Dauerbetrieb mit 1 String/min, Display aktiv, Funk aus. |
| A-07 | Ablesbarkeit | Display in direktem Sonnenlicht aus 1 m ablesbar (subjektiv, 3 Personen). |
| A-08 | Datenerhalt | 100 gespeicherte Strings und alle Einstellungen bleiben nach Akkuentnahme bzw. Tiefentladung erhalten. |
| A-09 | Kosten | Stückliste mit Bezugsquellen ≤ 40 € (Muss: ≤ 60 €). |

---

## 8. Vergleich mit Marktgeräten (Orientierung)

| Funktion | Typ. Marktgerät | Dieses Projekt V1 |
|---|---|---|
| Zufalls-/Fest-/Sofortstart | ✔ | ✔ (M) |
| Mehrere Par-Zeiten | ✔ | ✔ (S) |
| Spy-Modus | ✔ | ✔ (M) |
| Repetitive / Countdown | teilweise | ✔ (S) |
| Duell-/ISSF-Modus | selten | ✔ (S) |
| Stringspeicher | ✔ (oft ~100) | ✔ ≥ 100 (M) |
| Bluetooth-App | bei teureren Modellen | optional (K) |
| Hit Factor | teilweise | optional (K) |
| Akku mit USB-C | teilweise (oft AA/AAA) | ✔ (M) |
| Preis | ca. 100–250 € | Ziel ≤ 40 € Material |

---

## 9. Offene Punkte / Entscheidungen

1. **Mikrofonkonzept** (analoge Hüllkurve + Komparator vs. I²S-MEMS mit Software-Erkennung) → Steckbrett-Prototyp und Test am Stand.
2. **Display** (Sharp Memory LCD vs. IPS-TFT vs. OLED) → Kosten gegen Ablesbarkeit abwägen.
3. **Bedienkonzept** (5 Tasten vs. Drehencoder + 2 Tasten).
4. ~~Akkuform~~ **Entschieden (v0.2):** flacher LiPo (103450, 2000 mAh).
5. ~~Welche Disziplinen haben Priorität?~~ **Entschieden (v0.2):** IPSC-Training in der Halle und am Stand mit mehreren Schützen. Folgen: F-038 und F-039 → Muss, F-068 (Hit Factor) → Soll, F-066 (Duell) → Kann.
6. **App/Weboberfläche** in V1 oder erst in V2.

---

## 10. Nächste Schritte

1. Abstimmung und Freigabe dieser Anforderungen (Prioritäten prüfen, offene Punkte entscheiden).
2. Hardware-Konzept mit Blockschaltbild und Stückliste (BOM) inklusive Bezugsquellen.
3. Steckbrett-Prototyp: ESP32 + Mikrofon + Piezo → Test der Schusserkennung (kritischster Punkt).
4. Firmware-Architektur (Tasks, Zustandsautomat, Menüsystem, Speicherformat).
5. Schaltplan und Gehäuse, danach Praxistest und Abnahme nach Kapitel 7.
