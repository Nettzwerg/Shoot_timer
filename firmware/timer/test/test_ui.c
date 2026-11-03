// Host-Tests für die Bedienoberfläche (Navigation, Einstellungen, Historie, Wertung, Lernen).
#include <math.h>
#include <stdio.h>

#include "ui_fixture.h"

static int s_failed, s_checks;

#define CHECK(cond)                                                             \
    do {                                                                        \
        s_checks++;                                                             \
        if (!(cond)) {                                                          \
            s_failed++;                                                         \
            printf("  FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                       \
    } while (0)
#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        long long va_ = (long long)(a), vb_ = (long long)(b);                   \
        s_checks++;                                                             \
        if (va_ != vb_) {                                                       \
            s_failed++;                                                         \
            printf("  FEHLER %s:%d: %s == %lld, erwartet %lld\n", __FILE__,     \
                   __LINE__, #a, va_, vb_);                                     \
        }                                                                       \
    } while (0)
#define RUN(fn) do { printf("%s\n", #fn); fn(); } while (0)

static settings_t set;
static timer_core_t tc;
static ui_t ui;
static gfx_t g;

static void setup(void)
{
    memset(&fx_calls, 0, sizeof(fx_calls));
    settings_defaults(&set);
    tc_init(&tc, fx_rand, 1);
    ui_init(&ui, &set, &FX_ENV);
}

static void press_n(btn_key_t k, int n)
{
    for (int i = 0; i < n; i++) {
        fx_press(&ui, &tc, k);
    }
}

static void add_string(const double *t, int n)
{
    fx_play(&tc, &set, t, n);
    ui_string_done(&ui, &tc.last);
}

static void test_start_from_main(void)
{
    setup();
    fx_press(&ui, &tc, BTN_START);
    CHECK_EQ(fx_calls.start_keys, 1);
    // Im Menü startet START nicht
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.screen, UI_MENU);
    fx_press(&ui, &tc, BTN_START);
    CHECK_EQ(fx_calls.start_keys, 1);
}

static void test_keys_locked_while_running(void)
{
    setup();
    tc_start_key(&tc, &set, US(0));         // Verzögerung läuft
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_BACK);
    CHECK_EQ(ui.screen, UI_MAIN);
    fx_key(&ui, &tc, BTN_START, PRESS_LONG, 0);
    CHECK_EQ(fx_calls.power_off, 0);        // kein Ausschalten während des Laufs
    fx_press(&ui, &tc, BTN_START);
    CHECK_EQ(fx_calls.start_keys, 1);       // Abbruch geht an den Timer
}

static void test_edit_sensitivity(void)
{
    setup();
    fx_press(&ui, &tc, BTN_OK);             // Menü
    press_n(BTN_DOWN, 3);                   // Erkennung
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_OK);             // Empfindlichkeit bearbeiten
    CHECK_EQ(ui.screen, UI_EDIT);
    press_n(BTN_UP, 2);
    CHECK_EQ(set.sensitivity, 6);           // erst mit OK übernommen
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(set.sensitivity, 8);
    CHECK_EQ(fx_calls.changes, 1);
    CHECK_EQ(ui.screen, UI_MENU);
    // Verwerfen
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_UP, 5);                     // Grenze 10
    CHECK_EQ(ui.edit.value, 10);
    fx_press(&ui, &tc, BTN_BACK);
    CHECK_EQ(set.sensitivity, 8);
    fx_key(&ui, &tc, BTN_BACK, PRESS_LONG, 0);
    CHECK_EQ(ui.screen, UI_MAIN);
    CHECK_EQ(ui.depth, 0);
}

static void test_select_mode(void)
{
    setup();
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_OK);             // Modus
    press_n(BTN_DOWN, 5);                   // Countdown
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(set.mode, MODE_COUNTDOWN);
    CHECK_EQ(ui.depth, 1);                  // zurück im Hauptmenü
    CHECK(ui.toast[0] != '\0');
}

static void test_par_edit_with_repeat(void)
{
    setup();
    set.par_ms[0] = 2000;
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 2);                   // Par-Zeiten
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_DOWN);           // Par 1
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_UP);             // +0,01 s
    CHECK_EQ(ui.edit.value, 2010);
    btn_event_t ev = { .key = BTN_UP, .press = PRESS_REPEAT, .repeat = 20 };
    ui_key(&ui, &ev, &tc, 0);               // beschleunigt: +0,10 s
    CHECK_EQ(ui.edit.value, 2110);
    ev.repeat = 40;
    ui_key(&ui, &ev, &tc, 0);               // +0,50 s
    CHECK_EQ(ui.edit.value, 2610);
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(set.par_ms[0], 2610);
}

static void test_delay_min_max_consistent(void)
{
    setup();
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_DOWN);           // Startverzögerung
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_DOWN);           // Zufall von
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_UP, 40);                    // 1,0 -> 5,0 s
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(set.delay_min_ms, 5000);
    CHECK_EQ(set.delay_max_ms, 5000);       // "bis" mitgezogen
}

static void test_disabled_items(void)
{
    setup();
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 6);                   // Letzter String, ohne String gesperrt
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.depth, 1);
}

static void test_browse_and_delete(void)
{
    setup();
    const double t[] = { 1.0, 1.3, 1.5 };
    add_string(t, 3);
    CHECK_EQ(ui.hist_n, 1);
    CHECK_EQ(ui.shot_sel, 2);
    fx_press(&ui, &tc, BTN_DOWN);           // umlaufend
    CHECK_EQ(ui.shot_sel, 0);
    fx_press(&ui, &tc, BTN_UP);
    CHECK_EQ(ui.shot_sel, 2);

    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 6);                   // Letzter String
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_DOWN);           // Letzten Schuss löschen
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.hist[0].n, 2);
    CHECK_EQ(ui.shot_sel, 1);
}

static void test_history(void)
{
    setup();
    const double t[] = { 1.0, 1.3 };
    for (int i = 0; i < UI_HIST_MAX + 3; i++) {
        add_string(t, 2);
    }
    CHECK_EQ(ui.hist_n, UI_HIST_MAX);
    CHECK_EQ(ui.hist[0].seq, UI_HIST_MAX + 3);          // neuester zuerst
    CHECK_EQ(ui.hist[UI_HIST_MAX - 1].seq, 4);          // älteste verdrängt

    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 7);
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.screen, UI_HISTORY);
    press_n(BTN_DOWN, 2);
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.screen, UI_HVIEW);
    fx_press(&ui, &tc, BTN_OK);                         // übernehmen
    CHECK_EQ(ui.screen, UI_MAIN);
    CHECK_EQ(ui.cur, 2);
    // neuer String springt wieder auf den neuesten
    add_string(t, 2);
    CHECK_EQ(ui.cur, 0);
}

static void test_score(void)
{
    setup();
    set.power_factor = PF_MINOR;
    const double t[] = { 1.0, 1.5, 2.0, 2.5, 3.0, 4.0 };
    add_string(t, 6);
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 6);
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_OK);             // Wertung eingeben
    CHECK_EQ(ui.screen, UI_SCORE);
    CHECK_EQ(ui.score.v[SCORE_A], 6);       // vorbelegt mit Schusszahl
    press_n(BTN_DOWN, 2);                   // A 4
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_UP);             // C 1
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_UP);             // D 1
    press_n(BTN_OK, 3);                     // Miss, NS, Ergebnis
    CHECK(ui.score.result);
    fx_press(&ui, &tc, BTN_OK);             // speichern
    CHECK(ui.hist[0].scored);
    CHECK(fabsf(ss_hit_factor(&ui.hist[0]) - 6.0f) < 1e-4f);   // 24 Punkte / 4 s
    CHECK_EQ(ui.screen, UI_MAIN);
}

static void test_quick_sensitivity(void)
{
    setup();
    fx_press(&ui, &tc, BTN_BACK);
    CHECK_EQ(ui.screen, UI_QUICK);
    CHECK(fx_calls.listen_levels);
    press_n(BTN_DOWN, 7);
    CHECK_EQ(set.sensitivity, 1);
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.screen, UI_MAIN);
    CHECK(!fx_calls.listen_levels);
}

static void test_suggest(void)
{
    CHECK_EQ(ui_suggest_sensitivity(-30.0f), 5);    // Ziel -24 dBFS: Stufe 5 = -21,1
    CHECK_EQ(ui_suggest_sensitivity(-8.0f), 1);
    CHECK_EQ(ui_suggest_sensitivity(-60.0f), 10);
    for (uint8_t s = 1; s <= 10; s++) {
        float thr = settings_threshold_dbfs(s);
        uint8_t got = ui_suggest_sensitivity(thr - 6.0f);
        CHECK(settings_threshold_dbfs(got) >= thr - 0.001f);
    }
}

static void test_learn(void)
{
    setup();
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 3);
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 4);
    fx_key(&ui, &tc, BTN_OK, PRESS_SHORT, US(100));
    CHECK_EQ(ui.screen, UI_LEARN);
    CHECK(fx_calls.listen_raw);
    ui_raw_shot(&ui, -24.0f, US(101));
    ui_raw_shot(&ui, -30.0f, US(102));
    fx_key(&ui, &tc, BTN_OK, PRESS_SHORT, US(103));     // noch nicht fertig: ohne Wirkung
    CHECK_EQ(ui.screen, UI_LEARN);
    ui_tick(&ui, US(110));
    CHECK(ui.learn.done);
    CHECK(!fx_calls.listen_raw);
    CHECK_EQ(ui.learn.suggest, 4);                      // Ziel -18 dBFS: Stufe 4 = -17,3
    ui_raw_shot(&ui, -5.0f, US(111));                   // nach Ablauf ignoriert
    CHECK_EQ(ui.learn.count, 2);
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(set.sensitivity, 4);
    CHECK_EQ(ui.screen, UI_MENU);
}

static void test_power_and_factory(void)
{
    setup();
    fx_key(&ui, &tc, BTN_START, PRESS_LONG, 0);
    CHECK_EQ(ui.screen, UI_OFF);
    CHECK_EQ(fx_calls.power_off, 1);
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.screen, UI_OFF);
    fx_press(&ui, &tc, BTN_START);
    CHECK_EQ(ui.screen, UI_MAIN);
    CHECK_EQ(fx_calls.power_on, 1);
    CHECK_EQ(fx_calls.start_keys, 0);       // Einschalten startet keinen String

    set.sensitivity = 9;
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 9);                   // System
    fx_press(&ui, &tc, BTN_OK);
    press_n(BTN_DOWN, 5);                   // Werkseinstellungen
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(ui.screen, UI_CONFIRM);
    fx_press(&ui, &tc, BTN_BACK);
    CHECK_EQ(set.sensitivity, 9);
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_OK);
    CHECK_EQ(set.sensitivity, 6);
    CHECK_EQ(ui.screen, UI_MAIN);
}

static void test_render_all_screens(void)
{
    // Jeder Bildschirm muss in die Befehlsliste passen
    setup();
    const double t[] = { 1.0, 1.3, 1.5, 1.7, 1.9, 2.1, 2.3, 2.5, 2.7, 2.9 };
    add_string(t, 10);
    for (int s = UI_MAIN; s <= UI_OFF; s++) {
        ui.screen = (ui_screen_t)s;
        ui.depth = 1;
        ui.menu[0].id = 0;
        ui_render(&ui, &tc, 0, &g);
        CHECK(!g.overflow);
    }
    ui.screen = UI_MAIN;
    set.mode = MODE_PAR;
    set.par_count = PAR_MAX;
    tc_start_key(&tc, &set, US(500));
    ui_render(&ui, &tc, US(501), &g);
    CHECK(!g.overflow);
    tc_tick(&tc, tc.delay_end_us);
    for (int i = 0; i < SHOTS_MAX - 1; i++) {
        tc_shot(&tc, tc.t0_us + US(0.2 * (i + 1)), -4);
    }
    ui_render(&ui, &tc, tc.t0_us + US(25), &g);
    CHECK(g.n > 0);     // Zeitleiste mit 98 Schüssen und 10 Par-Marken
    CHECK(!g.overflow);
}

static void test_gfx_text_and_render(void)
{
    static uint16_t buf[GFX_W * 40];
    gfx_begin(&g, 0);
    gfx_text(&g, &font_f20, 160, 30, GFX_CENTER, 0xffff, "Äußerst ▲▼◄●");
    CHECK(g.ops[0].w > 0);
    CHECK_EQ(g.ops[0].x, 160 - g.ops[0].w / 2);
    gfx_render(&g, 0, 40, buf, false);
    int lit = 0;
    for (int i = 0; i < GFX_W * 40; i++) {
        lit += buf[i] != 0;
    }
    CHECK(lit > 100);
    gfx_render(&g, 200, 40, buf, false);    // Streifen ohne Text bleibt leer
    lit = 0;
    for (int i = 0; i < GFX_W * 40; i++) {
        lit += buf[i] != 0;
    }
    CHECK_EQ(lit, 0);
    // Byte-Tausch für das Display
    gfx_begin(&g, GFX_RGB(255, 0, 0));
    gfx_render(&g, 0, 1, buf, true);
    CHECK_EQ(buf[0], 0x00f8);
}

int main(void)
{
    RUN(test_start_from_main);
    RUN(test_keys_locked_while_running);
    RUN(test_edit_sensitivity);
    RUN(test_select_mode);
    RUN(test_par_edit_with_repeat);
    RUN(test_delay_min_max_consistent);
    RUN(test_disabled_items);
    RUN(test_browse_and_delete);
    RUN(test_history);
    RUN(test_score);
    RUN(test_quick_sensitivity);
    RUN(test_suggest);
    RUN(test_learn);
    RUN(test_power_and_factory);
    RUN(test_render_all_screens);
    RUN(test_gfx_text_and_render);
    printf("\n%d Prüfungen, %d Fehler\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
