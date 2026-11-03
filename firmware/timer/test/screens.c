// Rendert alle Bildschirme der UI am PC als PPM-Bilder (für Doku und Sichtprüfung).
//   make -C firmware/timer/test screens   -> docs/ui-screens/*.png
#include <stdio.h>
#include <stdlib.h>

#include "ui_fixture.h"

static gfx_t g;
static uint16_t strip[GFX_W * GFX_H];
static const char *s_outdir = ".";

static void save(const char *name, ui_t *ui, const timer_core_t *tc, int64_t now)
{
    ui_render(ui, tc, now, &g);
    if (g.overflow) {
        fprintf(stderr, "%s: zu viele Zeichenbefehle\n", name);
        exit(1);
    }
    // in 6 Streifen rastern, wie auf dem Gerät
    for (int y = 0; y < GFX_H; y += 40) {
        gfx_render(&g, y, 40, strip + y * GFX_W, false);
    }
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.ppm", s_outdir, name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) {
        uint16_t c = strip[i];
        unsigned char px[3] = { (unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 0x3f) << 2),
                                (unsigned char)((c & 0x1f) << 3) };
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    printf("%s\n", path);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        s_outdir = argv[1];
    }
    static settings_t set;
    static timer_core_t tc;
    static ui_t ui;
    settings_defaults(&set);
    set.mode = MODE_PAR;
    set.par_ms[0] = 2500;
    tc_init(&tc, fx_rand, 11);
    ui_init(&ui, &set, &FX_ENV);

    save("01_bereit", &ui, &tc, 0);

    const double t1[] = { 1.18, 1.41, 1.62, 1.84, 2.06, 2.31 };
    fx_play(&tc, &set, t1, 6);
    ui_string_done(&ui, &tc.last);
    ui.shot_sel = 3;
    save("02_ergebnis", &ui, &tc, 0);

    const double t2[] = { 1.32, 1.58, 1.81, 2.12, 2.44, 2.79 };
    fx_play(&tc, &set, t2, 6);
    ui_string_done(&ui, &tc.last);
    save("03_ergebnis_par_ueberschritten", &ui, &tc, 0);

    // Verzögerung und Lauf
    tc_start_key(&tc, &set, US(200));
    save("04_verzoegerung", &ui, &tc, US(201));
    tc_tick(&tc, tc.delay_end_us);
    int64_t t0 = tc.t0_us;
    tc_shot(&tc, t0 + US(1.21), -4);
    tc_shot(&tc, t0 + US(1.45), -5);
    tc_shot(&tc, t0 + US(1.68), -4);
    save("05_lauf", &ui, &tc, t0 + US(1.9));
    tc_start_key(&tc, &set, t0 + US(2));
    ui_string_done(&ui, &tc.last);

    // Countdown
    settings_t cd = set;
    cd.mode = MODE_COUNTDOWN;
    tc_start_key(&tc, &cd, US(300));
    tc_tick(&tc, tc.delay_end_us);
    tc_shot(&tc, tc.t0_us + US(3), -4);
    save("06_countdown", &ui, &tc, tc.t0_us + US(12.6));
    tc_start_key(&tc, &cd, tc.t0_us + US(13));

    // Pause (Wiederholung)
    settings_t rep = set;
    rep.mode = MODE_REPEAT;
    tc_start_key(&tc, &rep, US(400));
    tc_tick(&tc, tc.delay_end_us);
    tc_shot(&tc, tc.t0_us + US(1.5), -4);
    tc_tick(&tc, tc.t0_us + US(8));     // Auto-Stopp 5 s nach der Par-Zeit 2,5 s
    save("07_pause", &ui, &tc, tc.pause_end_us - US(3.2));
    tc_start_key(&tc, &rep, tc.pause_end_us - US(3));

    // Menü, Untermenü, Werteditor
    fx_press(&ui, &tc, BTN_OK);
    save("08_menue", &ui, &tc, 0);
    fx_press(&ui, &tc, BTN_DOWN);
    fx_press(&ui, &tc, BTN_DOWN);
    fx_press(&ui, &tc, BTN_OK);
    set.par_count = 3;
    set.par_ms[1] = 4000;
    set.par_ms[2] = 5500;
    save("09_par_zeiten", &ui, &tc, 0);
    fx_press(&ui, &tc, BTN_DOWN);
    fx_press(&ui, &tc, BTN_OK);
    save("10_werteditor", &ui, &tc, 0);
    fx_press(&ui, &tc, BTN_BACK);
    fx_press(&ui, &tc, BTN_BACK);
    fx_press(&ui, &tc, BTN_BACK);

    // Schnelleinstellung mit Pegel
    fx_press(&ui, &tc, BTN_BACK);
    ui_level(&ui, -31.0f, false);
    ui_level(&ui, -18.5f, false);
    ui_level(&ui, -27.0f, false);
    save("11_schnelleinstellung", &ui, &tc, 0);
    fx_press(&ui, &tc, BTN_OK);

    // Umgebung lernen
    fx_press(&ui, &tc, BTN_OK);
    for (int i = 0; i < 3; i++) {
        fx_press(&ui, &tc, BTN_DOWN);
    }
    fx_press(&ui, &tc, BTN_OK);     // Erkennung
    for (int i = 0; i < 4; i++) {
        fx_press(&ui, &tc, BTN_DOWN);
    }
    fx_key(&ui, &tc, BTN_OK, PRESS_SHORT, US(1000));
    ui_raw_shot(&ui, -24.0f, US(1002));
    ui_raw_shot(&ui, -19.5f, US(1004));
    ui_level(&ui, -40.0f, false);
    save("12_lernen", &ui, &tc, US(1006.2));
    ui_tick(&ui, US(1010.1));
    save("13_lernen_vorschlag", &ui, &tc, US(1010.2));
    fx_press(&ui, &tc, BTN_BACK);
    fx_key(&ui, &tc, BTN_BACK, PRESS_LONG, 0);

    // Historie
    fx_press(&ui, &tc, BTN_OK);
    for (int i = 0; i < 7; i++) {
        fx_press(&ui, &tc, BTN_DOWN);
    }
    fx_press(&ui, &tc, BTN_OK);
    save("14_historie", &ui, &tc, 0);
    fx_key(&ui, &tc, BTN_BACK, PRESS_LONG, 0);

    // Wertung: Menü -> Letzter String -> Wertung eingeben
    fx_press(&ui, &tc, BTN_OK);
    for (int i = 0; i < 6; i++) {
        fx_press(&ui, &tc, BTN_DOWN);
    }
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_DOWN);     // A 3 -> 2
    fx_press(&ui, &tc, BTN_OK);
    fx_press(&ui, &tc, BTN_UP);       // C 1
    save("15_wertung", &ui, &tc, 0);
    for (int i = 0; i < 4; i++) {
        fx_press(&ui, &tc, BTN_OK);
    }
    save("16_wertung_ergebnis", &ui, &tc, 0);
    fx_key(&ui, &tc, BTN_OK, PRESS_SHORT, US(2000));
    save("17_ergebnis_mit_hf_und_meldung", &ui, &tc, US(2000.5));
    return 0;
}
