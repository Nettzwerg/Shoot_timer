// Presets: gespeicherte Drills (Modus, Startverzögerung, Par-Zeiten, Modus-Optionen).
// Empfindlichkeit, Beep, Helligkeit usw. gehören nicht zum Drill und bleiben beim Laden erhalten.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "settings.h"

#define PRESET_SLOTS    10

// Übernimmt die Drill-Felder aus preset in dst
void preset_apply(settings_t *dst, const settings_t *preset);

// Kurzname aus dem Inhalt, z. B. "Par 2.50 s", "Countdown 30 s", "Spy"
void preset_name(const settings_t *preset, char *buf, size_t len);
