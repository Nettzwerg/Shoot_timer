// Bedienoberfläche: Bildschirme, Menü, Werteditor, Historie, Wertung, Mikrofon-Test.
// Umsetzung des UI-Prototyps (docs/ui-prototyp.html). Plattformunabhängig: Die Firmware
// liefert Tastenereignisse, Mikrofonpegel und einen Schnappschuss des Timers; die UI
// zeichnet in eine gfx-Befehlsliste und meldet Wünsche über die Rückrufe in ui_env_t.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buttons.h"
#include "gfx.h"
#include "settings.h"
#include "shot_string.h"
#include "timer_core.h"

#define UI_HIST_MAX     20      // Strings im RAM (dauerhafte Historie folgt mit M5)
#define UI_MENU_DEPTH   4
#define UI_FW_VERSION   "0.3.0"

typedef struct {
    void (*start_key)(void *ctx);                   // START an den Timer weitergeben
    void (*settings_changed)(void *ctx);            // Einstellungen speichern und anwenden
    void (*power)(void *ctx, bool on);              // Anzeige aus/an (M6: Deep-Sleep)
    void (*beep_test)(void *ctx);
    void (*listen)(void *ctx, bool levels, bool raw_shots);  // Pegelmeldungen / Rohdetektionen
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
} ui_screen_t;

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

    // Historie, neuester String an Index 0
    shot_string_t hist[UI_HIST_MAX];
    uint8_t hist_n;
    uint8_t cur;            // String im Hauptbildschirm
    uint8_t shot_sel;       // gewählter Schuss
    uint8_t hist_sel;
    uint8_t hview_sel;      // Schuss in der Historienansicht

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
    float battery_v;        // 0 = unbekannt
} ui_t;

void ui_init(ui_t *ui, settings_t *set, const ui_env_t *env);

// Tastenereignis. snap: aktueller Zustand des Timers (für Sperren während des Laufs).
void ui_key(ui_t *ui, const btn_event_t *ev, const timer_core_t *snap, int64_t now_us);

void ui_string_done(ui_t *ui, const shot_string_t *s);   // fertigen String übernehmen
void ui_level(ui_t *ui, float db, bool clipped);         // Pegelmeldung (ca. 10 Hz)
void ui_raw_shot(ui_t *ui, float db, int64_t now_us);    // Rohdetektion für „Umgebung lernen“
void ui_tick(ui_t *ui, int64_t now_us);                  // Zeitabläufe (Lernen, Pegelabfall)

void ui_render(ui_t *ui, const timer_core_t *snap, int64_t now_us, gfx_t *g);

// true, solange sich die Anzeige laufend ändert (Uhr, Pegel) und oft neu gezeichnet werden soll
bool ui_animating(const ui_t *ui, const timer_core_t *snap);

// Empfindlichkeitsvorschlag: höchste Stufe, deren Schwelle mindestens 6 dB über max_db liegt
uint8_t ui_suggest_sensitivity(float max_db);
