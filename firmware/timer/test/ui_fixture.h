// Gemeinsame Hilfen für UI-Tests und Screenshot-Programm (nur Host).
#pragma once

#include <string.h>

#include "settings.h"
#include "shot_string.h"
#include "timer_core.h"
#include "ui.h"

#define US(s) ((int64_t)((s) * 1000000.0 + 0.5))

typedef struct {
    int start_keys, changes, power_on, power_off, beeps, listen_levels, listen_raw;
} fx_calls_t;

static fx_calls_t fx_calls;
static uint32_t fx_rand(void) { return 1500; }

static void fx_start(void *c) { (void)c; fx_calls.start_keys++; }
static void fx_changed(void *c) { (void)c; fx_calls.changes++; }
static void fx_power(void *c, bool on) { (void)c; if (on) fx_calls.power_on++; else fx_calls.power_off++; }
static void fx_beep(void *c) { (void)c; fx_calls.beeps++; }
static void fx_listen(void *c, bool lv, bool raw) { (void)c; fx_calls.listen_levels = lv; fx_calls.listen_raw = raw; }

static const ui_env_t FX_ENV = { fx_start, fx_changed, fx_power, fx_beep, fx_listen, NULL };

static void fx_key(ui_t *ui, const timer_core_t *tc, btn_key_t k, btn_press_t p, int64_t now)
{
    btn_event_t ev = { .key = k, .press = p, .repeat = 0 };
    ui_key(ui, &ev, tc, now);
}

static void fx_press(ui_t *ui, const timer_core_t *tc, btn_key_t k)
{
    fx_key(ui, tc, k, PRESS_SHORT, 0);
}

// Spielt einen String mit Schüssen zu den Zeiten t[] (s ab Beep) durch den Kern
static void fx_play(timer_core_t *tc, const settings_t *s, const double *t, int n)
{
    tc_start_key(tc, s, US(100));
    tc_tick(tc, tc->delay_end_us);
    int64_t t0 = tc->t0_us;
    for (int i = 0; i < n; i++) {
        tc_shot(tc, t0 + US(t[i]), (int8_t)(-4 - i % 3));
    }
    tc_start_key(tc, s, t0 + US(t[n - 1] + 0.5));
}
