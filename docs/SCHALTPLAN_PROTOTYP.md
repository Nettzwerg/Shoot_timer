# Shot Timer – Steckbrett-Schaltpläne und Messablauf (Prototyp Phase 1–3)

| | |
|---|---|
| **Dokument** | Schaltpläne für den Steckbrett-Prototyp, Inbetriebnahme, Messablauf |
| **Version** | 0.3 (Entwurf) – Messung am Stand ohne Laptop, Live-Anzeige am Smartphone (WLAN) |
| **Datum** | 2026-10-25 |
| **Bezug** | [HARDWARE_KONZEPT.md](HARDWARE_KONZEPT.md) v0.2, Mess-Firmware [`firmware/mess`](../firmware/mess), Logger [`tools/mess_logger.py`](../tools/mess_logger.py) |

> Alle Bauteilwerte sind **Startwerte**. Mikrofondämpfung und Verstärkung werden mit den Messungen aus Phase 1 festgelegt (Kap. 6).

---

## 1. Überblick

| Phase | Aufbau | Versorgung | Ziel |
|---|---|---|---|
| **1** | DevKitC-1 + Mikrofon-Eingangsstufe (Kap. 2) | USB vom Laptop | Pegel eigener Schuss gegenüber Nachbarschuss in der Halle messen |
| **2** | + Beep-Stufe (Kap. 3) | USB vom Laptop | Piezo-Typ und Frequenz auswählen, Lautstärke messen |
| **3** | + Akku, Display, Tasten (Kap. 4) | LiPo | Funktionsmuster für die eigentliche Firmware |

In Phase 1 und 2 versorgt der USB-Anschluss des Dev-Boards alles: **3V3-Pin** für den Analogteil, **5V-Pin** für den Boostwandler. Ein Akku ist noch nicht nötig.

---

## 2. Mikrofon-Eingangsstufe (Phase 1)

### 2.1 Schaltplan

```
 Versorgung Analogteil (abschaltbar)
 ───────────────────────────────────
  3V3 (DevKit) ──────────┬──────────────── S
                         │              Q1 AO3401 (P-MOSFET)
                       R14 100k ─────── G ◄──────────────── GPIO 6   (Low = an)
                                        D
                                        │
                                        └──── AVCC ── C9 10 µF ── GND

 Referenzspannung VREF = AVCC / 2 ≈ 1,65 V
 ─────────────────────────────────────────
  AVCC ── R12 10k ──┬── R13 10k ── GND
                    ├── C7 10 µF ── GND
                    └── VREF

 Mikrofon und Vorverstärker U1A (G ≈ 1,5 → Kanal LOW)
 ───────────────────────────────────────────────────
  AVCC ── R1 1k ──┬── R2 2,2k ──┬──────── C1 1 µF ──┬──────────── U1A + (Pin 3)
                  │             │                   │
             C2 100 µF        MIC (+)            R3 100k
                  │           MIC (−)               │
                 GND            │                  VREF
                               GND

                     ┌─────────────── R5 4,7k ───────────────┐
                     │                                       │
  U1A − (Pin 2) ─────┴── R4 10k ── C3 150 nF ── VREF         │
  U1A Ausgang (Pin 1) ───────────────────────────────────────┴──── NODE_A

 Verstärker U1B (× 11 → Kanal HIGH)
 ─────────────────────────────────
  NODE_A ─────────────────────────────────────────────────────── U1B + (Pin 5)
                     ┌─────────────── R6 100k ───────────────┐
  U1B − (Pin 6) ─────┴── R7 10k ── C4 150 nF ── VREF         │
  U1B Ausgang (Pin 7) ───────────────────────────────────────┴──── NODE_B

 Präzisionsgleichrichter / Hüllkurve (zweimal identisch)
 ──────────────────────────────────────────────────────
                         D1 BAT54
  NODE_A ── U1C + (Pin 10)          U1C Ausgang (Pin 8) ──►|──┬──────── ENV_L ── R9 1k ── GPIO 1
            U1C − (Pin 9) ◄────────────────────────────────────┤
                                                               ├── C5 100 nF ── GND
                                                               └── R8 22k ──── GND

                         D2 BAT54
  NODE_B ── U1D + (Pin 12)          U1D Ausgang (Pin 14) ─►|──┬──────── ENV_H ── R11 1k ── GPIO 2
            U1D − (Pin 13) ◄───────────────────────────────────┤
                                                               ├── C6 100 nF ── GND
                                                               └── R10 22k ─── GND

 U1 = MCP6004:  Pin 4 = AVCC, Pin 11 = GND, C8 100 nF direkt zwischen Pin 4 und Pin 11
```

### 2.2 Bauteilliste Eingangsstufe

| Ref. | Wert | Funktion |
|---|---|---|
| U1 | MCP6004-I/P (DIP-14) | 4 × OPV, Rail-to-Rail, 1 MHz |
| MIC | Elektret-Kapsel Ø 9,7 mm, 2-polig, Empfindlichkeit möglichst gering (z. B. −44 bis −46 dBV/Pa) | Schallaufnahme |
| Q1 | AO3401 (SOT-23 auf Adapter) oder anderer P-MOSFET mit Logikpegel | Analogteil abschalten |
| D1, D2 | BAT54 (oder 1N5817 bzw. BAT85 bedrahtet) | Gleichrichterdioden (Schottky) |
| R1 | 1 kΩ | Siebung Mikrofon-Bias |
| R2 | 2,2 kΩ | Arbeitswiderstand Elektret |
| R3 | 100 kΩ | Arbeitspunkt U1A |
| R4, R7 | 10 kΩ | Verstärkung |
| R5 | 4,7 kΩ | Verstärkung LOW: G = 1 + R5/R4 = **1,47** |
| R6 | 100 kΩ | Verstärkung U1B: G = 1 + R6/R7 = **11** → HIGH gesamt 16,2 |
| R8, R10 | 22 kΩ | Entladung Hüllkurve, τ = 2,2 ms |
| R9, R11 | 1 kΩ | Schutz ADC-Eingang |
| R12, R13 | 10 kΩ | Teiler VREF |
| R14 | 100 kΩ | Q1 im Ruhezustand aus |
| C1 | 1 µF Folie/Keramik | Koppelkondensator |
| C2 | 100 µF Elko | Siebung Bias |
| C3, C4 | 150 nF | Hochpass ≈ 106 Hz (gegen Griff- und Windgeräusche) |
| C5, C6 | 100 nF | Hüllkurven-Speicher |
| C7, C9 | 10 µF | VREF bzw. AVCC |
| C8 | 100 nF | Abblockkondensator U1 |

### 2.3 Auslegung

- **Zwei Kanäle:** NODE_A (LOW, Verstärkung 1,47) übersteuert erst bei ca. 1,1 V Spitze an der Kapsel, NODE_B (HIGH, 16,2) bereits bei ca. 0,1 V. Das Verhältnis 11 (≈ 21 dB) ist der Standardwert für den Befehl `k` der Mess-Firmware. Der Logger berechnet den tatsächlichen Wert aus den Messungen.
- **Hüllkurve:** Die Superdiode lädt C5/C6 in wenigen µs auf die positive Spitze. R8/R10 entladen mit τ = 2,2 ms. Ruhepegel ≈ VREF (ADC ≈ 2000–2200), Vollausschlag ≈ 2,9–3,0 V. Ab ca. 2,7 V (ADC 3500) wertet die Firmware HIGH als übersteuert und rechnet mit LOW × k weiter.
- **Hochpass ~100 Hz** an beiden Verstärkerstufen. Der Gleichanteil bleibt auf VREF, ein Schuss (Hauptenergie 0,5–5 kHz) passiert ungehindert.
- **Bandbreite:** U1B mit G = 11 → ca. 90 kHz. Völlig ausreichend für die Hüllkurve.
- **Abschaltung:** Q1 hält den Analogteil im Deep-Sleep stromlos (Pull-up R14). Die Mess-Firmware schaltet GPIO 6 beim Start auf Low.

### 2.4 Akustische Dämpfung (Varianten für Phase 1)

Die Kapsel sitzt in einem kleinen Halter, z. B. einem gedruckten Röhrchen. Gemessen werden drei Varianten:

| Variante | Aufbau | erwartete Dämpfung |
|---|---|---|
| A | Kapsel offen | 0 dB (Referenz, wird beim eigenen Schuss wahrscheinlich übersteuern) |
| B | Kapsel hinter 5 mm offenporigem Schaumstoff | gering, einige dB |
| C | Kapsel in geschlossener Kammer (ca. Ø 12 × 8 mm), Zugang nur über eine Öffnung Ø 1 mm | deutlich, Größenordnung 10–20 dB |

Für den Standtermin B und C als **aufsteckbare Kappen** für den Mikrofonhalter drucken. Dann lässt sich die Variante ohne Werkzeug wechseln.

### 2.5 Steckbrett-Hinweise

- Analog-Masse sternförmig an **einem** GND-Pin des DevKits. Mikrofon-Masse und Masse von C5/C6 direkt dorthin.
- Mikrofon über eine 20–30 cm lange **abgeschirmte Leitung** (oder ein verdrilltes Paar) anschließen, damit es am Stand frei positioniert werden kann.
- C8 direkt an U1. Leitungen von ENV_L/ENV_H zum DevKit kurz halten.
- Der Beep-Teil (Phase 2) kommt auf ein **eigenes** Steckbrett und bekommt eine eigene GND-Leitung zum DevKit.

### 2.6 Inbetriebnahme (ohne Schießstand)

1. Ohne U1 einschalten: AVCC ≈ 3,3 V, VREF ≈ 1,65 V, an der Kapsel (MIC +) ca. 1,5–2,5 V.
2. U1 einsetzen: NODE_A, NODE_B ≈ 1,65 V; ENV_L, ENV_H ≈ 1,65–1,75 V.
3. Mess-Firmware flashen (Kap. 5), Befehl `s`: Ruhelage LOW/HIGH ≈ 2000–2200.
4. Befehl `l 1` (Pegelmonitor) und in die Hände klatschen: Der Pegel steigt deutlich, HIGH übersteuert bei lautem Klatschen nahe am Mikrofon.
5. Befehl `c 1` und klatschen: Ein Mitschnitt wird ausgegeben; mit `mess_logger.py plot` anzeigen und die steile Anstiegsflanke prüfen.

---

## 3. Beep-Stufe (Phase 2)

### 3.1 Schaltplan

```
 Boost ein-/ausschalten (High-Side)
 ──────────────────────────────────
  5V (DevKit-Pin 5V; ab Phase 3: VSYS) ──┬─────────────────── S
                                          │                 Q2 AO3401
                                        R15 100k ─────┬──── G
                                                      │     D ───── MT3608 VIN+
                                                      D
                                                   Q3 2N7002
  GPIO 42 ── R16 1k ──┬────────────────────────────── G
                      R17 100k                        S
                      │                               │
                     GND                             GND        MT3608 VIN− ── GND

 Boost und H-Brücke
 ──────────────────
  MT3608 VOUT+ (10,0 V) ──┬── C10 100 µF / 16 V ── GND
                          └── DRV8833 VM

  GPIO 39 ──────────── DRV8833 AIN1
  GPIO 40 ──────────── DRV8833 AIN2
  GPIO 41 ──────────── DRV8833 nSLEEP (auf dem Modul oft "SLP", "EEP" oder "STBY")
  GND ──────────────── DRV8833 GND, BIN1, BIN2

  DRV8833 AOUT1 ── R18 10 Ω ── Piezo ── DRV8833 AOUT2
```

### 3.2 Bauteilliste Beep-Stufe

| Ref. | Wert | Funktion |
|---|---|---|
| MT3608-Modul | Boostwandler 2 A, Trimmpoti | 5 V bzw. Akku → 10 V |
| DRV8833-Modul | Zweikanal-H-Brücke (nur Kanal A genutzt) | Vollbrücke für den Piezo |
| Q2 | AO3401 | Boost-Versorgung abschalten |
| Q3 | 2N7002 (oder BS170 bedrahtet) | Pegelwandler für Q2 |
| R15, R17 | 100 kΩ | Pull-up Q2 / Pull-down Q3 |
| R16 | 1 kΩ | Gate-Vorwiderstand |
| R18 | 10 Ω | begrenzt die Umladespitzen der Piezo-Kapazität |
| C10 | 100 µF / 16 V | Puffer VM |
| Piezo | passiv, ohne Oszillator, 3–4 kHz, 2–3 Typen | Signalgeber |

### 3.3 Inbetriebnahme – Reihenfolge einhalten

1. **MT3608 einstellen, bevor der DRV8833 angeschlossen wird.** Ausgang ohne Last messen und das Trimmpoti auf **10,0 V** drehen. Viele Module liefern ab Werk deutlich mehr Spannung; der DRV8833 verträgt maximal 10,8 V. Für die Messung GPIO 42 dauerhaft auf High legen (die Firmware schaltet den Boost beim Start ein).
2. **nSLEEP-Pin des DRV8833-Moduls prüfen:** Manche Module ziehen diesen Pin mit einem Widerstand an VM hoch. Spannung am offenen Pin messen. Liegt sie über 3,6 V, den Pull-up-Widerstand entfernen, bevor GPIO 41 angeschlossen wird.
3. DRV8833 und Piezo anschließen, Befehl `b 3000 300 2` (leise). Danach `b 3000 300 0` (voll).
4. **Gehörschutz tragen.** Bei voller Lautstärke nahe am Ohr kann ein Piezo über 100 dB erreichen.

### 3.4 Lautstärke-Stufen (Firmware)

| Stufe | Ansteuerung | Spannung am Piezo |
|---|---|---|
| 0 voll | AIN1 und AIN2 gegenphasig (Vollbrücke) | ±10 V = 20 Vss |
| 1 halb | AIN2 dauerhaft High, AIN1 PWM 50 % | 10 Vss (−6 dB) |
| 2 leise | wie 1, Tastgrad 10 % | Grundwelle ca. −10 dB |

---

## 4. Funktionsmuster (Phase 3): Stromversorgung, Display, Tasten

### 4.1 Stromversorgung mit Akku

```
  USB-C ── TP4056-Modul ── B+ / B− ── LiPo 2000 mAh (mit Schutzplatine)
              │ IN+ (5 V)       │ OUT+ (geschützt)
              │                 │
              │                SW1 Schiebeschalter
              │                 │
              │                 ├── R20 470k ──┬── R21 470k ── GND
              │                 │              ├── C11 100 nF ── GND
              │                 │              └── GPIO 4 (Akkuspannung)
              │                 │
              │                 D  Q4 AO3401   (Drain = Akkuseite)
              │                 S ─────────────────────┬────────── VSYS
              ├── G (Q4) ── R19 100k ── GND            │
              │                                        │
              └────────────────► D3 SS14 ──────────────┘

  VSYS ── C12 10 µF ── HT7833 VIN     HT7833 VOUT ── C13 10 µF ── J1 ── DevKit 3V3
                         GND ── GND
```

- **Load-Sharing:** Ist USB angesteckt, liegt das Gate von Q4 auf 5 V und Q4 sperrt. Das System läuft dann über D3 aus USB, und der TP4056 lädt den Akku ohne Last sauber bis zum Abschalten. Ohne USB zieht R19 das Gate auf GND, Q4 leitet und der Akku versorgt das System.
- **SW1** trennt nur den Akku. Mit USB lädt der Akku auch bei ausgeschaltetem Gerät.
- **J1 (Steckbrücke)** muss **offen** sein, solange das DevKit über seinen eigenen USB-Anschluss versorgt wird (zum Programmieren). Sonst arbeiten zwei Spannungsregler gegeneinander bzw. es fließt Strom rückwärts in den Regler des DevKits. Auf der eigenen Platine (V2) entfällt das, weil es dort nur eine USB-Buchse gibt.
- Die **Power-LED des DevKits** auslöten, wenn der Ruhestrom gemessen werden soll.
- Der Boostwandler (Kap. 3) wird ab Phase 3 an **VSYS** statt an 5V angeschlossen.

### 4.2 Display (IPS-TFT 2,0", ST7789, SPI)

| Display-Pin | DevKit | Bemerkung |
|---|---|---|
| GND | GND | |
| VCC | 3V3 | |
| SCL | GPIO 12 | SPI-Takt |
| SDA | GPIO 11 | SPI-Daten (MOSI) |
| RES | GPIO 8 | |
| DC | GPIO 9 | |
| CS | GPIO 10 | |
| BLK | GPIO 7 | PWM; prüfen, ob das Modul einen Transistor für die Beleuchtung hat. Sonst N-MOSFET 2N7002 vorschalten. |

### 4.3 Tasten

Alle Taster zwischen GPIO und GND, interne Pull-ups:

| Taste | GPIO |
|---|---|
| START | 14 |
| AUF | 15 |
| AB | 16 |
| OK/MENÜ | 17 |
| ZURÜCK | 18 |

---

## 5. Mess-Firmware und Logger

### 5.1 Bauen und Flashen

Die Mess-Firmware liegt unter [`firmware/mess`](../firmware/mess) (ESP-IDF über PlatformIO).

1. [VS Code](https://code.visualstudio.com/) mit der Erweiterung **PlatformIO IDE** installieren.
2. Ordner `firmware/mess` öffnen.
3. DevKit über den USB-Port **„UART“** anschließen (nicht „USB“).
4. PlatformIO: **Upload**. Beim ersten Mal lädt PlatformIO das ESP-IDF automatisch herunter, das dauert einige Minuten.

Alternativ in der Kommandozeile: `pio run -t upload` im Ordner `firmware/mess`.

> Stand: Der Quellcode (inkl. Standalone-Modus) wurde mit ESP-IDF 5.3.1 (Ziel ESP32-S3) ohne Fehler und Warnungen kompiliert. Auf echter Hardware ist er noch **nicht** getestet.

### 5.2 Logger am Laptop (zu Hause; am Stand optional)

```
pip install pyserial matplotlib
python tools/mess_logger.py log --port COM5          (Linux: /dev/ttyUSB0 oder /dev/ttyACM0)
```

Alles, was der Timer sendet, landet mit Zeitstempel in `messungen/messung_<Datum>.log`, Mitschnitte als eigene CSV-Dateien. Eingaben werden an den Timer gesendet; Eingaben mit `!` am Anfang werden als **Markierung** ins Log geschrieben.

### 5.3 Befehle

| Befehl | Wirkung |
|---|---|
| `h` | Hilfe |
| `s` | Status: Schwelle, Ruhelage, DMA-Überläufe, Beep-Einstellung |
| `e 0/1` | Schussmeldungen aus/ein (Standard: ein) |
| `l 0/1` | Pegelmonitor mit 10 Hz |
| `c 0/1` | Mitschnitt je Schuss (50 ms vor bis 300 ms nach der Schwelle) |
| `t <dBFS>` | Schwelle (Standard −35) |
| `d <ms>` | Echo-/Totzeit (Standard 50) |
| `k <Faktor>` | Verhältnis HIGH/LOW (Standard 11) |
| `b [Hz] [ms] [Stufe]` | Beep, Werte bleiben gespeichert (Standard 3000 Hz, 300 ms, voll) |
| `w <f1> <f2> <Schritt>` | Frequenz-Sweep des Beeps mit Pegelmessung am Mikrofon |
| `g [ms]` | Timer-Durchgang: Verzögerung (Standard zufällig 2–4 s), Beep, Schusszeiten relativ zum Beep |
| `x` | Timer-Durchgang beenden |
| `p 0/1` | Boostwandler aus/ein |
| `a 0/1` | Analogteil aus/ein |
| `m` | nächste Situation markieren (wie Taste BOOT kurz) |
| `i` | Speicherbelegung im Flash |
| `F` | alle gespeicherten Sitzungen ausgeben (nutzt `mess_logger.py download`) |
| `X` | alle gespeicherten Sitzungen löschen, Situationszähler auf 0 |
| `n 0/1` | WLAN-Weboberfläche aus/ein (gespeichert, wirkt nach Neustart; Standard: ein) |

Ausgabe je Schuss (CSV): `SHOT,<Nr>,<Zeit s>,<Split s>,<Pegel dBFS>,<Anstieg ms>,<LOW>,<HIGH>,<HIGH übersteuert>`.

> Die Mess-Firmware ordnet den Beep auf ca. ±2 ms genau der Sample-Zeitachse zu. Das reicht für die Messungen; die eigentliche Timer-Firmware startet den Beep synchron zur Erfassung.

---

## 6. Messablauf Phase 1 am Stand

### 6.1 Standalone-Betrieb – kein Laptop nötig

Am Stand braucht es **keinen Laptop**. Die Mess-Firmware speichert alles im Flash des ESP32 (ca. 6 MB):

- jede Schussmeldung (`SHOT`), jeden Beep und jede Markierung,
- die **ersten 3 Mitschnitte nach jeder Markierung** (je 0,35 s Signalverlauf).

Ausgelesen wird zu Hause mit `mess_logger.py download`. Bedient wird der Aufbau am Stand nur über die Taste **BOOT** des DevKits:

| Taste BOOT | Wirkung | Quittung |
|---|---|---|
| beim Einschalten | – (nicht drücken, sonst startet der ESP32 im Programmiermodus) | **2 kurze Töne** = bereit, Speicher ok; **1 langer Ton** = Speicherfehler |
| kurz drücken | nächste **Situation** markieren (1, 2, 3 …) | Situationsnummer als Tonfolge: je 5 ein langer Ton, je 1 ein kurzer Ton (z. B. 7 = lang, kurz, kurz) |
| 1,5 s halten | **Timer-Durchgang**: zufällige Verzögerung 2–4 s, dann Start-Beep | Start-Beep |

- Der Situationszähler bleibt auch erhalten, wenn die Powerbank abgezogen wird. Er wird erst mit dem Befehl `X` (oder `download --erase`) zurückgesetzt.
- Jede Stromunterbrechung beginnt eine neue Sitzung im Speicher. Daten gehen dabei nicht verloren.
- Die Schwelle steht ab Werk auf −45 dBFS (niedrig), damit auch Nachbarschüsse erfasst werden.
- **Voraussetzung:** Die Beep-Stufe (Phase 2) muss aufgebaut sein, sonst gibt es keine Quittungstöne. Ohne Beep-Stufe funktioniert die Messung trotzdem, aber die Situationsnummern müssen dann gezählt werden.

**Vor dem Standtermin zu Hause:**

1. Firmware flashen, Aufbau prüfen (Kap. 2.6), Speicher leeren: `python tools/mess_logger.py download --port COM5 --erase`.
2. Probelauf mit Klatschen: einschalten → 2 Töne, BOOT kurz → 1 Ton, klatschen, BOOT kurz → 2 Töne, klatschen. Auslesen und mit `summary` prüfen.
3. Mit dem iPhone verbinden (Kap. 6.2a), WLAN-Störungen prüfen, Seite zum Home-Bildschirm hinzufügen.
4. Messprotokoll (Kap. 6.3) ausdrucken.

**Aufbau am Stand:** Steckbrett und Powerbank in eine kleine Box (z. B. Frischhaltedose) hinter der Feuerlinie. Das Mikrofon hängt an der 1–2 m langen Leitung und wird je Situation positioniert. **Erlaubnis der Standaufsicht einholen.** Mikrofon und Box nur bei freigegebener Bahn umstellen.

> **Powerbank:** Manche Powerbanks schalten bei geringer Stromaufnahme (< 50–100 mA) nach einigen Sekunden ab. Vorher zu Hause testen: läuft der Aufbau 15 min durch? Sonst eine Powerbank mit „Low-Current“- bzw. Kleingerätemodus verwenden.

### 6.2 Ablauf

1. Powerbank anstecken → 2 kurze Töne abwarten.
2. Für jede Zeile des Messprotokolls: Situation herstellen, **BOOT kurz** drücken, Tonfolge mitzählen, dann die angegebenen Schüsse abgeben. Den Haken im Protokoll setzen.
3. Dämpfungsvariante wechseln (Kappe auf dem Mikrofonhalter tauschen, Kap. 2.4) und mit der nächsten Situation weitermachen.
4. Am Ende einige **Timer-Durchgänge** (BOOT 1,5 s halten) mit Doublette oder Bill Drill, um die Split-Erkennung zu prüfen. Vorher eine eigene Situation markieren.
5. Zu Hause auslesen und auswerten:

   ```
   python tools/mess_logger.py download --port COM5
   python tools/mess_logger.py summary messungen/flash_*.log --protokoll protokoll.txt
   python tools/mess_logger.py plot messungen/cap_flash_<...>.csv
   ```

   `protokoll.txt` enthält die Zeilen des Messprotokolls im Format `1 = A eigen 9mm 0.5m`.

### 6.2a Live-Anzeige am Smartphone (iPhone und Android)

Die Mess-Firmware öffnet ein eigenes WLAN mit einer Webseite. Dafür ist **keine App** nötig, nur der Browser (Safari, Chrome). Eine Internetverbindung braucht es auch nicht.

1. Timer einschalten (2 kurze Töne).
2. iPhone: *Einstellungen → WLAN* → **ShotTimer-Mess**, Passwort **shoot1234**. Den Hinweis „Keine Internetverbindung“ bestätigen bzw. im WLAN bleiben.
3. In Safari **http://192.168.4.1** öffnen. Tipp: *Teilen → Zum Home-Bildschirm*, dann ist die Seite am Stand mit einem Tipp erreichbar.

Die Seite zeigt:

- jeden Schuss sofort mit Nummer, Zeit, Split, Pegel und Anstiegszeit; **Ü** = Kanal HIGH übersteuert,
- die Situationen als Trennzeilen,
- die Knöpfe **„Nächste Situation“** und **„Timer-Durchgang“**. Sie wirken genau wie die BOOT-Taste, die Quittungstöne kommen trotzdem,
- einen Pegelbalken (Schalter „Pegel“), um die Schwelle vor Ort zu prüfen,
- unter *Einstellungen und Daten*: Schwelle, Totzeit, Test-Beep und **„Alle gespeicherten Daten herunterladen“**.

Alles wird zusätzlich im Flash gespeichert. Bricht die WLAN-Verbindung ab, geht nichts verloren.

**Daten nach Hause bringen** – drei Wege, alle gleichwertig:

| Weg | Vorgehen |
|---|---|
| USB am PC | `python tools/mess_logger.py download --port COM5` |
| iPhone | Link „Alle gespeicherten Daten herunterladen“ → Datei `shottimer_dump.txt` landet in der App *Dateien* → per AirDrop/iCloud/Mail auf den PC → `python tools/mess_logger.py download --datei shottimer_dump.txt` |
| PC per WLAN | PC mit „ShotTimer-Mess“ verbinden, http://192.168.4.1/api/dump im Browser speichern, weiter wie beim iPhone |

> **WLAN und Analogteil:** Die Sendeimpulse des WLAN können auf dem Steckbrett als Störung im Mikrofonsignal auftauchen. Die Sendeleistung ist deshalb auf 10 dBm reduziert. **Vor dem Standtermin prüfen:** In der Webseite den Pegel einschalten und ohne Geräusche beobachten. Die Ruhe sollte deutlich unter −50 dBFS liegen und keine Schussmeldungen auslösen. Danach mit `n 0` (WLAN aus, Neustart) vergleichen. Stört das WLAN, entweder Leitungen kürzen/abschirmen oder am Stand ohne WLAN messen.

Mit einem Android-Telefon geht alternativ auch eine USB-Terminal-App mit OTG-Kabel (z. B. „Serial USB Terminal“, 921600 Baud). Mit dem iPhone ist das nicht möglich, weil iOS keine USB-Seriell-Adapter unterstützt.

### 6.3 Messprotokoll (zum Ausdrucken)

| Nr. | Dämpfung | Situation | Mikrofon-Position | Schüsse | ✔ |
|---|---|---|---|---|---|
| 1 | A offen | eigener Schuss 9 mm | Gürtel des Schützen (ca. 0,5 m) | 5–10 | ☐ |
| 2 | A offen | eigener Schuss 9 mm | Hand der Standaufsicht, ca. 1 m hinter dem Schützen | 5–10 | ☐ |
| 3 | A offen | Nachbar 9 mm, eigener Schütze schießt nicht | wie Nr. 1, Nachbar eine Bahn weiter | 5–10 | ☐ |
| 4 | A offen | Nachbar 9 mm | wie Nr. 1, Nachbar zwei Bahnen weiter | 5–10 | ☐ |
| 5 | A offen | eigener Schuss .22 (falls vorhanden) | wie Nr. 1 | 5 | ☐ |
| 6–10 | B Schaumstoff | wie Nr. 1–5 | | | ☐ |
| 11–15 | C Kammer Ø 1 mm | wie Nr. 1–5 | | | ☐ |
| 16 | beste Variante | Timer-Durchgänge (BOOT lang): Doublette, Bill Drill | wie Nr. 1 | 3–5 Durchgänge | ☐ |

Abweichungen (andere Waffe, andere Bahn, Fehlbedienung) direkt in der Zeile notieren und später in `protokoll.txt` übernehmen.

### 6.4 Bewertung

| Kriterium | Ziel | Wenn nicht erreicht |
|---|---|---|
| Eigener Schuss 0,5 m: Pegel im Kanal LOW nicht am Anschlag (dBFS < −1, LOW-Rohwert deutlich unter ca. 1200) | ✔ | stärkere Dämpfung (Variante C) oder R5 verkleinern (z. B. 0 Ω → G = 1) |
| Median „eigen 0,5 m“ ≥ 8 dB über Maximum „nachbar 1 Bahn“ | ✔ | Platzierung prüfen; Rückfallebene externer Sensor (Hardware-Konzept Kap. 4.3) |
| Splits ≥ 0,10 s vollständig erkannt | ✔ | Totzeit (`d`) verkleinern, Mitschnitte auf Nachhall prüfen |
| Anstiegszeit eigen < nachbar | Hinweis | entscheidet, ob die Anstiegsprüfung in der Firmware sinnvoll ist |
| Gemessenes Verhältnis HIGH/LOW | ≈ 11 | Wert notieren, mit `k` übernehmen |

Die Ergebnisse (Summary-Ausgabe + einige Mitschnitte) bitte ins Repository unter `messungen/` legen. Daraus werden die endgültigen Werte für Dämpfung, R5/R6 und die Standardschwelle abgeleitet.

## 7. Messablauf Phase 2 (Beep)

Diese Messung findet **zu Hause** mit Laptop statt; am Stand ist dafür nichts nötig.

1. Piezo in ca. 30 cm Abstand vom Mikrofon (Variante A, ohne Dämpfung) aufstellen, `t 0` (keine Schussmeldungen).
2. `w 2000 5000 100` – Frequenzgang aufnehmen; `python tools/mess_logger.py sweep messungen/messung_*.log` zeigt die lauteste Frequenz.
3. Mit einem Schallpegelmesser (oder einer kalibrierten Smartphone-App) in 30 cm messen: `b <f_best> 1000 0`. Ziel ≥ 100 dB(A).
4. Für jeden Piezo-Typ wiederholen, den lautesten auswählen, Frequenz als Standard notieren.
5. Test unter Gehörschutz: Ist der Beep auch bei Schüssen auf der Nachbarbahn deutlich wahrnehmbar?
