// Erfassung der Mikrofon-Hüllkurve (LOW/HIGH) per ADC-DMA und Schusserkennung
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ENV_SAMPLE_RATE_HZ  20000   // je Kanal
#define ENV_SAMPLE_US       50
#define ENV_MS_TO_SAMPLES(ms) ((uint32_t)(ms) * (ENV_SAMPLE_RATE_HZ / 1000))

typedef struct {
    float threshold_dbfs;   // Schwelle relativ zum Vollausschlag des kombinierten Signals
    uint32_t dead_time_ms;  // Echo-/Totzeit nach einem Schuss
    float ratio;            // Verstärkungsverhältnis HIGH/LOW
} env_config_t;

typedef enum {
    ENV_EVT_SHOT,           // Schuss erkannt (nach Ablauf der Totzeit gemeldet)
    ENV_EVT_LEVEL,          // Pegelbericht (alle 100 ms, falls eingeschaltet)
    ENV_EVT_CAPTURE,        // Mitschnitt fertig, Daten über env_capture_data()
} env_evt_type_t;

typedef struct {
    env_evt_type_t type;
    uint32_t idx;           // Sample-Index (50 µs) des Ereignisbeginns
    float peak_dbfs;
    float rise_ms;          // Schwellenüberschreitung bis Spitze
    uint16_t peak_low;      // Rohwert-Spitze über Ruhelage
    uint16_t peak_high;
    bool high_clipped;      // HIGH übersteuert, Wert stammt aus LOW
} env_event_t;

void env_start(void);
env_config_t *env_config(void);
bool env_get_event(env_event_t *evt, uint32_t timeout_ms);

uint32_t env_now_idx(void);             // geschätzter aktueller Sample-Index
void env_blank_until(uint32_t idx);     // keine Schusserkennung bis idx
void env_set_level_report(bool on);
void env_set_capture(bool on);

// Mitschnitt: verschachtelt low,high; gültig bis env_capture_release()
const uint16_t *env_capture_data(uint32_t *n_pairs, uint32_t *start_idx);
void env_capture_release(void);

// Spitzenpegel in einem Zeitfenster messen (z. B. Beep-Sweep)
void env_window_start(uint32_t from_idx, uint32_t to_idx);
bool env_window_result(float *peak_dbfs);

float env_battery_v(void);
void env_status(float *base_low, float *base_high, uint32_t *overflows);
