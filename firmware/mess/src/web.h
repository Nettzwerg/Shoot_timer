// WLAN-Access-Point mit Weboberfläche für das Smartphone (iPhone/Android, nur Browser nötig).
// WLAN "ShotTimer-Mess", Passwort "shoot1234", Adresse http://192.168.4.1
#pragma once

#include <stdbool.h>

typedef void (*web_cmd_fn)(const char *line);

bool web_start(web_cmd_fn on_cmd);
void web_publish(const char *line);     // Zeile an die Live-Liste anhängen
void web_set_level(const char *line);   // letzte LVL-Zeile
void web_set_info(const char *text);    // Kopfzeile (Situation, Schwelle ...)
