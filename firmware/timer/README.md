# Timer-Firmware

Gerätesoftware des Shot-Timers. Architektur: [docs/FIRMWARE_ARCHITEKTUR.md](../../docs/FIRMWARE_ARCHITEKTUR.md).

## Stand: Meilenstein M2

| funktioniert | folgt |
|---|---|
| Alle 6 Modi (Standard, Par, Spy, Trockentraining, Wiederholung, Countdown) | Display und Menü (M3) |
| Startverzögerung, Start-, Par-, Warn- und Ende-Beep | Historie im Flash (M5) |
| Schusserkennung mit Zeitstempeln in µs, Empfindlichkeit und Echo-Zeit | Energieverwaltung, Deep-Sleep (M6) |
| Einstellungen werden im NVS gespeichert | WLAN, Export (M8) |
| Anzeige und Einstellungen über die serielle Konsole | |

## Aufbau

Läuft auf dem Steckbrett aus Phase 1 + 2 ([SCHALTPLAN_PROTOTYP.md](../../docs/SCHALTPLAN_PROTOTYP.md)). Zusätzlich optional die Tasten aus Kap. 4.3 (GPIO 14–18 gegen GND). **Ohne Tasten** wirkt die Taste **BOOT** des DevKits als START. Nicht beim Einschalten drücken, sonst startet der ESP32 im Programmiermodus.

## Bauen und Flashen

Wie die Mess-Firmware: VS Code mit PlatformIO, Ordner `firmware/timer` öffnen, **Upload**. Danach den **Monitor** öffnen (115200 Baud).

Die beiden Firmwares teilen den Flash unterschiedlich auf. Nach dem Wechsel von der Mess- zur Timer-Firmware sind die gespeicherten Messdaten nicht mehr lesbar. Vorher auslesen (`mess_logger.py download`).

## Bedienung über die Konsole

START drücken (oder `g` eingeben) startet einen String. Ausgabe:

```
STATE,Verzoegerung
STATE,Lauf
SHOT,1,1.18,1.18,-4
SHOT,2,1.41,0.23,-5
STRING,3,Par,2,1.18,1.41,0.23,0.23,START
STATE,Bereit
```

`STRING`: Nummer, Modus, Schüsse, erster Schuss, Gesamtzeit, schnellster Split, Ø-Split, Grund für das Ende.

| Befehl | Wirkung |
|---|---|
| `h` | Hilfe |
| `s` | Einstellungen anzeigen |
| `g` | wie START |
| `l` | letzten String anzeigen |
| `m 0`…`m 5` | Modus: 0 Standard, 1 Par, 2 Spy, 3 Trockentraining, 4 Wiederholung, 5 Countdown |
| `e 1`…`e 10` | Empfindlichkeit (10 = am empfindlichsten) |
| `d r 1 4` / `d f 3` / `d i` | Zufallsstart 1–4 s / fest 3 s / sofort |
| `p 2.5` / `p 1.5 3 4.5` | eine bzw. mehrere Par-Zeiten in s |
| `a 5` | Auto-Stopp nach 5 s ohne Schuss (0 = aus) |
| `o 50` | Echo-Zeit in ms |
| `c 30 10` | Countdown 30 s, Warnung 10 s vor Ende |
| `r 5` | Pause zwischen Durchgängen (Wiederholung, Trockentraining) |
| `n 10` | Anzahl Durchgänge Trockentraining |
| `b 3000 300 2` | Beep: Frequenz, Dauer, Lautstärke 0–2 |
| `w` | Werkseinstellungen |

Änderungen werden sofort gespeichert und gelten ab dem nächsten String.

## Tests

Der Kern (`src/core`) wird ohne Hardware am PC getestet:

```
make -C firmware/timer/test
```
