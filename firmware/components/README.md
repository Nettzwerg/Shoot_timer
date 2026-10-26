# Gemeinsame Komponenten

Werden von der Mess-Firmware (`../mess`) und der Timer-Firmware (`../timer`) genutzt.
Beide Projekte binden sie in ihrer `CMakeLists.txt` über `EXTRA_COMPONENT_DIRS` ein.

| Komponente | Inhalt |
|---|---|
| `board` | Pinbelegung (`board_pins.h`) |
| `envelope` | ADC-DMA, Hüllkurve LOW/HIGH, Schusserkennung |
| `beeper` | Piezo über DRV8833 und Boostwandler |
