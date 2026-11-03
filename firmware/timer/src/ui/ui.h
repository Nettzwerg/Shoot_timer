// Bedienoberfläche: Bildschirme, Menü, Werteditor, Historie, Wertung, Mikrofon-Test.
// Umsetzung des UI-Prototyps (docs/ui-prototyp.html). Plattformunabhängig: Die Firmware
// liefert Tastenereignisse, Mikrofonpegel und einen Schnappschuss des Timers; die UI
// zeichnet in eine gfx-Befehlsliste und meldet Wünsche über die Rückrufe in ui_env_t.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "gfx.h"
#include "history.h"
#include "power.h"
#include "presets.h"
#include "settings.h"
#include "shot_string.h"
#include "timer_core.h"

#define UI_MENU_DEPTH   4
#define UI_FW_VERSION   "0.6.0"

typedef struct {
    void (*start_key)(void *ctx);                   // START an den Timer weitergeben
    void (*settings_changed)(void *ctx);            // Einstellungen speichern und anwenden
    void (*power)(void *ctx, bool on);              // Anzeige aus/an (M6: Deep-Sleep)
    void (*beep_test)(void *ctx);
    void (*listen)(void *ctx, bool levels, bool raw_shots);  // Pegelmeldungen / Rohdetektionen
    void (*preset_save)(void *ctx, uint8_t slot, const settings_t *preset);
    void *ctx;
} ui_env_t;

typedef enum {
    UI_MAIN = 0,
    UI_MENU,
    UI_EDIT,
    UI_QUICK,
    UI_MICTEST,
    UI_LEARN,
    UI_HISTORY,
    UI_HVIEW,
    UI_SCORE,
    UI_CONFIRM,
    UI_OFF,
    UI_NOTICE,          // Hinweis vor dem Ausschalten (z. B. „Akku leer“)
} ui_screen_t;

typedef enum { CONFIRM_FACTORY = 0, CONFIRM_CLEAR_HISTORY } ui_confirm_t;

typedef struct {
    ui_env_t env;
    settings_t *set;
    ui_screen_t screen;

    struct {
        uint8_t id, sel, top;
    } menu[UI_MENU_DEPTH];
    uint8_t depth;

    struct {
        uint8_t field;
        int32_t value;
    } edit;

    history_t *hist;        // dauerhafte Historie, neuester String an Index 0
    shot_string_t cur;      // String im Hauptbildschirm
    bool has_cur;
    uint8_t shot_sel;       // gewählter Schuss
    uint16_t hist_sel;
    shot_string_t hview;    // String in der Historienansicht
    uint8_t hview_sel;      // Schuss in der Historienansicht
    ui_confirm_t confirm;

    settings_t presets[PRESET_SLOTS];
    bool preset_used[PRESET_SLOTS];
    char preset_label[PRESET_SLOTS][32];    // Menütexte "P1 Par 2.50 s"
    char save_label[PRESET_SLOTS][40];      // Menütexte "Platz 1: …"

    struct {
        uint8_t v[SCORE_FIELDS];
        uint8_t idx;
        bool result;
    } score;

    struct {
        int64_t end_us;
        float max_db;
        uint16_t count;
        bool done;
        uint8_t suggest;
    } learn;

    struct {
        float db, peak;
        bool clipped;
    } level;

    char toast[32];
    int64_t toast_until;
    float battery_v;
    uint8_t battery_pct;
    uint8_t battery_state;  // batt_state_t, BATT_NONE bis zur ersten Messung
    bool charging;
    char notice[40];
} ui_t;

// hist muss geöffnet sein; der neueste gespeicherte String erscheint im Hauptbildschirm.
void ui_init(ui_t *ui, settings_t *set, history_t *hist, const ui_env_t *env);
void ui_set_preset(ui_t *ui, uint8_t slot, const settings_t *preset);   // beim Start aus dem Speicher

// Tastenereignis. snap: aktueller Zustand des Timers (für Sperren während des Laufs).
void ui_key(ui_t *ui, const btn_event_t *ev, const timer_core_t *snap, int64_t now_us);

void ui_string_done(ui_t *ui, const shot_string_t *s);   // fertigen String speichern und anzeigen
void ui_level(ui_t *ui, float db, bool clipped);         // Pegelmeldung (ca. 10 Hz)
void ui_raw_shot(ui_t *ui, float db, int64_t now_us);    // Rohdetektion für „Umgebung lernen“
void ui_tick(ui_t *ui, int64_t now_us);                  // Zeitabläufe (Lernen, Pegelabfall)
void ui_battery(ui_t *ui, float v, uint8_t pct, batt_state_t state, bool charging);
void ui_message(ui_t *ui, const char *text, int64_t now_us);    // kurze Meldung oben
void ui_notice(ui_t *ui, const char *text);              // Hinweisbildschirm, z. B. vor dem Ausschalten

void ui_render(ui_t *ui, const timer_core_t *snap, int64_t now_us, gfx_t *g);

// true, solange sich die Anzeige laufend ändert (Uhr, Pegel) und oft neu gezeichnet werden soll
bool ui_animating(const ui_t *ui, const timer_core_t *snap);

// Empfindlichkeitsvorschlag: höchste Stufe, deren Schwelle mindestens 6 dB über max_db liegt
uint8_t ui_suggest_sensitivity(float max_db);
