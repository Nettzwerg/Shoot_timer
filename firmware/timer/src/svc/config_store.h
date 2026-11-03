// Einstellungen im NVS (Namespace "timer"), versioniert und auf Grenzen geprüft.
#pragma once

#include <stdbool.h>

#include "presets.h"
#include "settings.h"

void config_load(settings_t *s);        // fehlt oder ungültig -> Werkseinstellungen
bool config_save(const settings_t *s);

bool preset_load(uint8_t slot, settings_t *s);     // false, wenn der Platz leer ist
bool preset_store(uint8_t slot, const settings_t *s);
