// Einstellungen des Timers (Datenmodell, Grenzen, Werkseinstellungen).
// Plattformunabhängig: keine ESP-IDF-Abhängigkeiten, wird auf dem PC getestet.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SETTINGS_VERSION    1
#define PAR_MAX             10

typedef enum {
    MODE_STANDARD = 0,
    MODE_PAR,
    MODE_SPY,
    MODE_DRY,           // Trockentraining
    MODE_REPEAT,        // Wiederholung
    MODE_COUNTDOWN,
    MODE_COUNT,
} timer_mode_t;

typedef enum {
    DELAY_RANDOM = 0,
    DELAY_FIXED,
    DELAY_INSTANT,
    DELAY_COUNT,
} delay_type_t;

typedef enum {
    PF_MINOR = 0,
    PF_MAJOR,
} power_factor_t;

typedef struct {
    uint16_t version;

    uint8_t mode;               // timer_mode_t
    uint8_t delay_type;         // delay_type_t
    uint16_t delay_min_ms;      // Zufallsstart von
    uint16_t delay_max_ms;      // Zufallsstart bis
    uint16_t delay_fixed_ms;

    uint8_t par_count;          // 1..PAR_MAX
    uint32_t par_ms[PAR_MAX];   // ab Beep, in beliebiger Reihenfolge eingegeben
    bool par_in_repeat;         // Par-Zeiten auch im Modus Wiederholung

    uint8_t sensitivity;        // 1..10, 10 = am empfindlichsten
    uint16_t echo_ms;           // Totzeit nach einem Schuss
    uint8_t auto_stop_s;        // 0 = aus

    uint16_t beep_hz;
    uint16_t beep_ms;
    uint8_t beep_volume;        // 0 leise, 1 mittel, 2 laut

    uint8_t repeat_pause_s;
    uint16_t countdown_s;
    uint8_t countdown_warn_s;   // 0 = aus
    uint8_t dry_reps;

    uint8_t power_factor;       // power_factor_t
    uint8_t brightness;         // 20..100 %
    uint8_t auto_off_min;
    bool wifi;
} settings_t;

void settings_defaults(settings_t *s);

// Begrenzt alle Werte auf ihre zulässigen Bereiche. Gibt true zurück, wenn etwas geändert wurde.
bool settings_sanitize(settings_t *s);

// Schwelle der Schusserkennung in dBFS für eine Empfindlichkeitsstufe (1 = -6 dBFS ... 10 = -40 dBFS)
float settings_threshold_dbfs(uint8_t sensitivity);

// Aktive Par-Zeiten des Modus, aufsteigend sortiert. Gibt die Anzahl zurück (0 = keine).
uint8_t settings_active_pars(const settings_t *s, uint32_t out_ms[PAR_MAX]);
