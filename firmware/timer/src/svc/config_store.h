// Einstellungen im NVS (Namespace "timer"), versioniert und auf Grenzen geprüft.
#pragma once

#include <stdbool.h>

#include "settings.h"

void config_load(settings_t *s);        // fehlt oder ungültig -> Werkseinstellungen
bool config_save(const settings_t *s);
