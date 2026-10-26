// Bedientasten: Entprellen, kurzer/langer Druck, Wiederholung bei ▲/▼.
#pragma once

typedef enum {
    BTN_START = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_OK,
    BTN_BACK,
    BTN_COUNT,
} btn_key_t;

typedef enum {
    PRESS_SHORT = 0,    // START/OK/ZURÜCK: beim Loslassen; ▲/▼: sofort beim Drücken
    PRESS_LONG,         // START 2 s, ZURÜCK 1 s (einmal, noch während des Haltens)
    PRESS_REPEAT,       // ▲/▼ gehalten: nach 400 ms alle 90 ms
} btn_press_t;

typedef struct {
    btn_key_t key;
    btn_press_t press;
    unsigned repeat;    // Anzahl Wiederholungen bisher (für beschleunigtes Ändern)
} btn_event_t;

typedef void (*btn_cb_t)(const btn_event_t *ev);

// Startet den Abfrage-Task. Die Taste BOOT des DevKits wirkt zusätzlich als START.
void buttons_start(btn_cb_t cb);
