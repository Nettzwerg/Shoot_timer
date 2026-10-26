// Zustandsautomat des Timers: Verzögerung, Beep, Par, Auto-Stopp, Modi.
// Plattformunabhängig: Zeit kommt als Parameter (µs), Ergebnisse als Aktions-Bits.
// Der Aufrufer (timer-Task) führt die Aktionen aus: Beep starten, Erkennung schalten,
// fertigen String speichern, Anzeige aktualisieren.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"
#include "shot_string.h"

#define TC_MAX_TIME_US      999990000LL   // 999,99 s
#define TC_DRY_END_US       600000LL      // Trockentraining endet 0,6 s nach der letzten Par-Zeit
#define TC_DRY_NOPAR_US     3000000LL     // ... bzw. nach 3 s ohne Par-Zeit

typedef enum {
    TC_READY = 0,       // Hauptbildschirm (Bereit/Ergebnis)
    TC_DELAY,           // Startverzögerung läuft
    TC_RUN,             // Messung läuft
    TC_PAUSE,           // Pause zwischen zwei Durchgängen (Wiederholung, Trockentraining)
} tc_state_t;

// Aktions-Bits (Rückgabewert der tc_*-Funktionen)
#define TC_ACT_BEEP_START   (1u << 0)
#define TC_ACT_BEEP_PAR     (1u << 1)
#define TC_ACT_BEEP_WARN    (1u << 2)   // Countdown-Warnung
#define TC_ACT_BEEP_END     (1u << 3)   // Countdown-Ende
#define TC_ACT_DETECT_ON    (1u << 4)
#define TC_ACT_DETECT_OFF   (1u << 5)
#define TC_ACT_STRING_DONE  (1u << 6)   // tc->last enthält den fertigen String
#define TC_ACT_SHOT_ADDED   (1u << 7)
#define TC_ACT_CHANGED      (1u << 8)   // Anzeige neu zeichnen

typedef enum {
    TC_END_NONE = 0,
    TC_END_KEY,         // START gedrückt
    TC_END_AUTO_STOP,
    TC_END_COUNTDOWN,
    TC_END_DRY,         // Trockentraining nach letzter Par-Zeit
    TC_END_MAX_TIME,
    TC_END_FULL,        // 99 Schüsse
} tc_end_reason_t;

typedef struct {
    tc_state_t state;
    settings_t cfg;             // Kopie der Einstellungen beim Start der Serie
    uint32_t (*rand32)(void);

    int64_t delay_start_us;
    int64_t delay_end_us;
    int64_t t0_us;              // Start-Beep = Zeitpunkt 0 des Strings
    int64_t pause_end_us;
    uint8_t par_idx;            // nächste fällige Par-Zeit
    bool warned;
    uint8_t reps_left;          // verbleibende Trockentrainings-Durchgänge
    uint32_t seq;               // Nummer des nächsten Strings

    shot_string_t run;          // String in Arbeit
    shot_string_t last;         // zuletzt abgeschlossener String
    bool has_last;
    tc_end_reason_t end_reason;
} timer_core_t;

void tc_init(timer_core_t *tc, uint32_t (*rand32)(void), uint32_t next_seq);

// START-Taste (kurz). Startet, bricht ab oder beendet je nach Zustand.
uint32_t tc_start_key(timer_core_t *tc, const settings_t *settings, int64_t now_us);

// Erkannter Schuss mit absolutem Zeitstempel (µs, gleiche Zeitbasis wie now_us).
uint32_t tc_shot(timer_core_t *tc, int64_t shot_us, int8_t db);

// Regelmäßig bzw. zum Zeitpunkt aus tc_next_deadline() aufrufen.
uint32_t tc_tick(timer_core_t *tc, int64_t now_us);

// Nächster Zeitpunkt, zu dem tc_tick() etwas zu tun hat; INT64_MAX = keiner.
int64_t tc_next_deadline(const timer_core_t *tc);

// Tatsächlichen Beginn des Start-Beeps nachtragen (falls der Beep später als now_us begann).
void tc_set_t0(timer_core_t *tc, int64_t t0_us);

// Laufende Zeit seit dem Start-Beep (nur in TC_RUN sinnvoll).
uint32_t tc_elapsed_us(const timer_core_t *tc, int64_t now_us);
