// Energieverwaltung: Akkuzustand, Dimmen und automatisches Ausschalten.
// Plattformunabhängig: Spannung und Zeit kommen als Parameter, Ergebnisse als Aktions-Bits.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PWR_DIM_US          60000000LL  // nach 60 s ohne Bedienung dimmen
#define PWR_NO_BATT_V       2.8f        // darunter: kein Akku angeschlossen (USB-Betrieb)
#define PWR_LOW_V           3.40f       // Warnung
#define PWR_LOW_CLEAR_V     3.50f       // Warnung zurücknehmen (Hysterese, z. B. beim Laden)
#define PWR_CRIT_V          3.30f       // ausschalten ...
#define PWR_CRIT_HOLD_US    10000000LL  // ... wenn die Spannung 10 s lang darunter bleibt

typedef enum {
    BATT_NONE = 0,      // kein Akku (USB-Versorgung) oder Messung nicht verdrahtet
    BATT_OK,
    BATT_LOW,
    BATT_CRITICAL,
} batt_state_t;

// Aktions-Bits
#define PWR_ACT_DIM         (1u << 0)
#define PWR_ACT_UNDIM       (1u << 1)
#define PWR_ACT_OFF         (1u << 2)   // automatisches Ausschalten (Inaktivität)
#define PWR_ACT_LOW_WARN    (1u << 3)   // Akku schwach (einmal beim Eintritt)
#define PWR_ACT_SHUTDOWN    (1u << 4)   // Akku leer: sofort speichern und ausschalten

typedef struct {
    int64_t last_activity_us;
    bool dimmed;
    float v;                // gefilterte Akkuspannung
    bool v_valid;
    batt_state_t state;
    int64_t crit_since_us;  // 0 = Spannung nicht unter PWR_CRIT_V
    bool warned;
    bool shutdown_sent;
} power_t;

void pwr_init(power_t *p, int64_t now_us);

// Bedienung (Taste) oder Schuss. Liefert PWR_ACT_UNDIM, wenn die Anzeige gedimmt war.
uint32_t pwr_activity(power_t *p, int64_t now_us);

// Regelmäßig aufrufen. busy: Timer läuft oder eine Live-Anzeige ist offen (kein Dimmen, kein Aus).
uint32_t pwr_tick(power_t *p, int64_t now_us, bool busy, uint8_t auto_off_min);

// Neue Spannungsmessung (ca. 1 × pro Sekunde).
uint32_t pwr_battery(power_t *p, float v, int64_t now_us);

// Ladezustand in Prozent aus der Ruhespannung einer LiPo-Zelle (grobe Kennlinie).
uint8_t pwr_percent(float v);
