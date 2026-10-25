// Ablage der Messdaten im Flash (SPIFFS), damit am Stand kein Laptop nötig ist.
// Jeder Start legt eine neue Sitzung an: l<NNN>.txt (Textzeilen) und c<NNN>_<MMM>.bin (Mitschnitte).
#pragma once

#include <stdbool.h>
#include <stdint.h>

bool store_init(void);
bool store_ok(void);
uint32_t store_session(void);
void store_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void store_capture(uint32_t trigger_idx, uint32_t start_idx, const uint16_t *data, uint32_t pairs);
void store_info(void);
typedef void (*store_write_fn)(const char *data, void *ctx);
void store_dump(void);      // alle Sitzungen über die Konsole ausgeben
void store_dump_to(store_write_fn write, void *ctx);   // dito über eigene Ausgabe (z. B. HTTP)
void store_erase(void);     // alle Sitzungen löschen, neue Sitzung beginnen
