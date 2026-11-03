#include "ui.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// ---------- Farben und Schriften (wie im Prototyp) ----------

#define C_BG        GFX_RGB(0, 0, 0)
#define C_FG        GFX_RGB(242, 242, 238)
#define C_MUTED     GFX_RGB(139, 145, 140)
#define C_DIM       GFX_RGB(58, 63, 60)
#define C_PANEL     GFX_RGB(20, 23, 21)
#define C_ACC       GFX_RGB(255, 122, 46)
#define C_ON_ACC    GFX_RGB(22, 8, 0)
#define C_GOOD      GFX_RGB(74, 222, 128)
#define C_BAD       GFX_RGB(255, 90, 79)

#define F_LABEL     (&font_f12)
#define F_SMALL     (&font_f16)
#define F_TEXT      (&font_f20)
#define F_VALUE     (&font_f26)
#define F_LARGE     (&font_f44)
#define F_HUGE      (&font_f84)

#define LEARN_US    10000000LL
#define TOAST_US    1600000LL

static const char *const MODE_NAMES[MODE_COUNT] = {
    "Standard", "Par", "Spy", "Trockentraining", "Wiederholung", "Countdown",
};

// ---------- Einstellbare Felder ----------

enum {
    F_DELAY_TYPE, F_DELAY_MIN, F_DELAY_MAX, F_DELAY_FIXED,
    F_PAR_COUNT, F_PAR0, F_PAR_LAST = F_PAR0 + PAR_MAX - 1, F_PAR_REP,
    F_SENS, F_ECHO, F_AUTOSTOP,
    F_BEEP_HZ, F_BEEP_MS, F_BEEP_VOL,
    F_REP_PAUSE, F_CD_TIME, F_CD_WARN, F_DRY_REPS,
    F_PF, F_BRIGHT, F_AUTOOFF, F_WIFI,
    F_COUNT,
};

typedef struct {
    uint16_t offset;
    uint8_t size;               // Bytes im settings_t
    int32_t min, max, step;     // in gespeicherten Einheiten
    uint16_t div;               // Anzeige = Wert / div
    uint8_t dec;                // Nachkommastellen der Anzeige
    const char *unit;
    const char *const *opts;    // Auswahlfeld: Texte je Wert
    uint8_t nopts;
    const char *zero_text;      // Text für den Wert 0 (z. B. "Aus")
} field_t;

static const char *const OPT_DELAY[] = { "Zufall", "Fest", "Sofort" };
static const char *const OPT_ONOFF[] = { "Aus", "Ein" };
static const char *const OPT_VOL[] = { "leise", "mittel", "laut" };
static const char *const OPT_PF[] = { "Minor", "Major" };

#define FLD(member) offsetof(settings_t, member), sizeof(((settings_t *)0)->member)

static const field_t FIELDS[F_COUNT] = {
    [F_DELAY_TYPE] = { FLD(delay_type), 0, DELAY_COUNT - 1, 1, 1, 0, "", OPT_DELAY, 3, NULL },
    [F_DELAY_MIN] = { FLD(delay_min_ms), 500, 10000, 100, 1000, 1, "s", NULL, 0, NULL },
    [F_DELAY_MAX] = { FLD(delay_max_ms), 500, 10000, 100, 1000, 1, "s", NULL, 0, NULL },
    [F_DELAY_FIXED] = { FLD(delay_fixed_ms), 0, 10000, 100, 1000, 1, "s", NULL, 0, NULL },
    [F_PAR_COUNT] = { FLD(par_count), 1, PAR_MAX, 1, 1, 0, "", NULL, 0, NULL },
    [F_PAR_REP] = { FLD(par_in_repeat), 0, 1, 1, 1, 0, "", OPT_ONOFF, 2, NULL },
    [F_SENS] = { FLD(sensitivity), 1, 10, 1, 1, 0, "", NULL, 0, NULL },
    [F_ECHO] = { FLD(echo_ms), 20, 500, 10, 1, 0, "ms", NULL, 0, NULL },
    [F_AUTOSTOP] = { FLD(auto_stop_s), 0, 60, 1, 1, 0, "s", NULL, 0, "Aus" },
    [F_BEEP_HZ] = { FLD(beep_hz), 1000, 4000, 100, 1, 0, "Hz", NULL, 0, NULL },
    [F_BEEP_MS] = { FLD(beep_ms), 100, 1000, 50, 1, 0, "ms", NULL, 0, NULL },
    [F_BEEP_VOL] = { FLD(beep_volume), 0, 2, 1, 1, 0, "", OPT_VOL, 3, NULL },
    [F_REP_PAUSE] = { FLD(repeat_pause_s), 3, 60, 1, 1, 0, "s", NULL, 0, NULL },
    [F_CD_TIME] = { FLD(countdown_s), 5, 300, 5, 1, 0, "s", NULL, 0, NULL },
    [F_CD_WARN] = { FLD(countdown_warn_s), 0, 30, 5, 1, 0, "s", NULL, 0, "Aus" },
    [F_DRY_REPS] = { FLD(dry_reps), 1, 20, 1, 1, 0, "", NULL, 0, NULL },
    [F_PF] = { FLD(power_factor), 0, 1, 1, 1, 0, "", OPT_PF, 2, NULL },
    [F_BRIGHT] = { FLD(brightness), 20, 100, 10, 1, 0, "%", NULL, 0, NULL },
    [F_AUTOOFF] = { FLD(auto_off_min), 5, 60, 5, 1, 0, "min", NULL, 0, NULL },
    [F_WIFI] = { FLD(wifi), 0, 1, 1, 1, 0, "", OPT_ONOFF, 2, NULL },
};

static field_t field_def(uint8_t id)
{
    if (id >= F_PAR0 && id <= F_PAR_LAST) {
        field_t f = { offsetof(settings_t, par_ms) + (id - F_PAR0) * sizeof(uint32_t), sizeof(uint32_t),
                      100, 99990, 10, 1000, 2, "s", NULL, 0, NULL };
        return f;
    }
    return FIELDS[id];
}

static int32_t field_get(const settings_t *s, uint8_t id)
{
    field_t f = field_def(id);
    const uint8_t *p = (const uint8_t *)s + f.offset;
    switch (f.size) {
    case 1: {
        uint8_t v;
        memcpy(&v, p, 1);
        return v;
    }
    case 2: {
        uint16_t v;
        memcpy(&v, p, 2);
        return v;
    }
    default: {
        uint32_t v;
        memcpy(&v, p, 4);
        return (int32_t)v;
    }
    }
}

static void field_set(settings_t *s, uint8_t id, int32_t v)
{
    field_t f = field_def(id);
    uint8_t *p = (uint8_t *)s + f.offset;
    switch (f.size) {
    case 1: {
        uint8_t x = (uint8_t)v;
        memcpy(p, &x, 1);
        break;
    }
    case 2: {
        uint16_t x = (uint16_t)v;
        memcpy(p, &x, 2);
        break;
    }
    default: {
        uint32_t x = (uint32_t)v;
        memcpy(p, &x, 4);
        break;
    }
    }
}

static void field_format(uint8_t id, int32_t v, char *buf, size_t len)
{
    field_t f = field_def(id);
    if (f.opts) {
        snprintf(buf, len, "%s", (v >= 0 && v < f.nopts) ? f.opts[v] : "?");
    } else if (f.zero_text && v == 0) {
        snprintf(buf, len, "%s", f.zero_text);
    } else if (f.div == 1) {
        snprintf(buf, len, "%ld%s%s", (long)v, *f.unit ? " " : "", f.unit);
    } else {
        snprintf(buf, len, "%.*f%s%s", f.dec, (double)v / f.div, *f.unit ? " " : "", f.unit);
    }
}

// ---------- Menüs ----------

enum { M_MAIN, M_MODE, M_DELAY, M_PAR, M_DETECT, M_BEEP, M_MODEOPT, M_LAST, M_PRESETS, M_SYSTEM };
enum { IT_SUB, IT_FIELD, IT_ACT, IT_MODE, IT_INFO, IT_PRESET };
enum { A_HISTORY, A_MICTEST, A_LEARN, A_BEEPTEST, A_SCORE, A_DELLAST, A_FACTORY };
enum { I_BATTERY, I_FIRMWARE };

static const char *const MENU_TITLES[] = {
    "Menü", "Modus", "Startverzögerung", "Par-Zeiten", "Erkennung", "Beep",
    "Modus-Optionen", "Letzter String", "Presets", "System",
};

typedef struct {
    const char *label;
    uint8_t type;
    uint8_t arg;
    bool disabled;
} item_t;

#define MENU_MAX_ITEMS 16

typedef struct {
    const char *name;
    uint8_t mode, delay_type;
    uint16_t delay_min_ms, delay_max_ms;
    uint32_t par_ms;
    uint8_t dry_reps;
} preset_t;

static const preset_t PRESETS[] = {
    { "Bill Drill", MODE_PAR, DELAY_RANDOM, 2000, 4000, 2500, 0 },
    { "El Presidente", MODE_PAR, DELAY_RANDOM, 2000, 4000, 10000, 0 },
    { "Doublette", MODE_PAR, DELAY_RANDOM, 1000, 3000, 1500, 0 },
    { "Ziehen trocken", MODE_DRY, DELAY_RANDOM, 2000, 4000, 1200, 10 },
};
#define N_PRESETS (sizeof(PRESETS) / sizeof(PRESETS[0]))

static shot_string_t *current(ui_t *ui)
{
    return ui->hist_n ? &ui->hist[ui->cur] : NULL;
}

static int build_menu(ui_t *ui, uint8_t id, item_t *it)
{
    const settings_t *s = ui->set;
    int n = 0;
#define ADD(l, t, a, d) (it[n++] = (item_t) { (l), (t), (a), (d) })
    switch (id) {
    case M_MAIN:
        ADD("Modus", IT_SUB, M_MODE, false);
        ADD("Startverzögerung", IT_SUB, M_DELAY, false);
        ADD("Par-Zeiten", IT_SUB, M_PAR, false);
        ADD("Erkennung", IT_SUB, M_DETECT, false);
        ADD("Beep", IT_SUB, M_BEEP, false);
        ADD("Modus-Optionen", IT_SUB, M_MODEOPT, false);
        ADD("Letzter String", IT_SUB, M_LAST, current(ui) == NULL);
        ADD("Historie", IT_ACT, A_HISTORY, false);
        ADD("Presets", IT_SUB, M_PRESETS, false);
        ADD("System", IT_SUB, M_SYSTEM, false);
        break;
    case M_MODE:
        for (int m = 0; m < MODE_COUNT; m++) {
            ADD(MODE_NAMES[m], IT_MODE, m, false);
        }
        break;
    case M_DELAY:
        ADD("Art", IT_FIELD, F_DELAY_TYPE, false);
        ADD("Zufall von", IT_FIELD, F_DELAY_MIN, s->delay_type != DELAY_RANDOM);
        ADD("Zufall bis", IT_FIELD, F_DELAY_MAX, s->delay_type != DELAY_RANDOM);
        ADD("Fest", IT_FIELD, F_DELAY_FIXED, s->delay_type != DELAY_FIXED);
        break;
    case M_PAR: {
        static const char *const names[PAR_MAX] = {
            "Par 1", "Par 2", "Par 3", "Par 4", "Par 5", "Par 6", "Par 7", "Par 8", "Par 9", "Par 10",
        };
        ADD("Anzahl", IT_FIELD, F_PAR_COUNT, false);
        for (int i = 0; i < s->par_count && i < PAR_MAX; i++) {
            ADD(names[i], IT_FIELD, F_PAR0 + i, false);
        }
        ADD("In Wiederholung", IT_FIELD, F_PAR_REP, false);
        break;
    }
    case M_DETECT:
        ADD("Empfindlichkeit", IT_FIELD, F_SENS, false);
        ADD("Echo-Zeit", IT_FIELD, F_ECHO, false);
        ADD("Auto-Stopp", IT_FIELD, F_AUTOSTOP, false);
        ADD("Mikrofon-Test", IT_ACT, A_MICTEST, false);
        ADD("Umgebung lernen", IT_ACT, A_LEARN, false);
        break;
    case M_BEEP:
        ADD("Tonhöhe", IT_FIELD, F_BEEP_HZ, false);
        ADD("Dauer", IT_FIELD, F_BEEP_MS, false);
        ADD("Lautstärke", IT_FIELD, F_BEEP_VOL, false);
        ADD("Test-Beep", IT_ACT, A_BEEPTEST, false);
        break;
    case M_MODEOPT:
        ADD("Pause Wiederholung", IT_FIELD, F_REP_PAUSE, false);
        ADD("Countdown-Zeit", IT_FIELD, F_CD_TIME, false);
        ADD("Countdown-Warnung", IT_FIELD, F_CD_WARN, false);
        ADD("Trocken-Durchgänge", IT_FIELD, F_DRY_REPS, false);
        break;
    case M_LAST: {
        const shot_string_t *c = current(ui);
        ADD("Wertung eingeben", IT_ACT, A_SCORE, c == NULL || c->n == 0);
        ADD("Letzten Schuss löschen", IT_ACT, A_DELLAST, c == NULL || c->n == 0);
        ADD("Power Factor", IT_FIELD, F_PF, false);
        break;
    }
    case M_PRESETS:
        for (unsigned i = 0; i < N_PRESETS; i++) {
            ADD(PRESETS[i].name, IT_PRESET, i, false);
        }
        break;
    case M_SYSTEM:
        ADD("Helligkeit", IT_FIELD, F_BRIGHT, false);
        ADD("Auto-Aus", IT_FIELD, F_AUTOOFF, false);
        ADD("WLAN", IT_FIELD, F_WIFI, false);
        ADD("Akku", IT_INFO, I_BATTERY, false);
        ADD("Firmware", IT_INFO, I_FIRMWARE, false);
        ADD("Werkseinstellungen", IT_ACT, A_FACTORY, false);
        break;
    }
#undef ADD
    return n;
}

static void item_value(const ui_t *ui, const item_t *it, char *buf, size_t len)
{
    const settings_t *s = ui->set;
    buf[0] = '\0';
    switch (it->type) {
    case IT_FIELD:
        field_format(it->arg, field_get(s, it->arg), buf, len);
        break;
    case IT_INFO:
        if (it->arg == I_FIRMWARE) {
            snprintf(buf, len, "%s", UI_FW_VERSION);
        } else if (ui->battery_v > 0) {
            snprintf(buf, len, "%.2f V", (double)ui->battery_v);
        } else {
            snprintf(buf, len, "–");
        }
        break;
    case IT_SUB:
        switch (it->arg) {
        case M_MODE:
            snprintf(buf, len, "%s", MODE_NAMES[s->mode]);
            break;
        case M_DELAY:
            if (s->delay_type == DELAY_RANDOM) {
                snprintf(buf, len, "%.1f–%.1f s", s->delay_min_ms / 1000.0, s->delay_max_ms / 1000.0);
            } else if (s->delay_type == DELAY_FIXED) {
                snprintf(buf, len, "%.1f s", s->delay_fixed_ms / 1000.0);
            } else {
                snprintf(buf, len, "Sofort");
            }
            break;
        case M_PAR:
            if (s->par_count == 1) {
                snprintf(buf, len, "%.2f s", s->par_ms[0] / 1000.0);
            } else {
                snprintf(buf, len, "%u Zeiten", s->par_count);
            }
            break;
        case M_DETECT:
            snprintf(buf, len, "Empf. %u", s->sensitivity);
            break;
        case M_BEEP:
            snprintf(buf, len, "%.1f kHz, %s", s->beep_hz / 1000.0, OPT_VOL[s->beep_volume > 2 ? 2 : s->beep_volume]);
            break;
        case M_LAST:
            if (ui->hist_n) {
                snprintf(buf, len, "#%lu", (unsigned long)ui->hist[ui->cur].seq);
            }
            break;
        }
        break;
    case IT_ACT:
        if (it->arg == A_HISTORY) {
            snprintf(buf, len, "%u", ui->hist_n);
        }
        break;
    }
}

// ---------- Hilfsfunktionen ----------

static void toast(ui_t *ui, const char *text, int64_t now_us)
{
    snprintf(ui->toast, sizeof(ui->toast), "%s", text);
    ui->toast_until = now_us + TOAST_US;
}

static void changed(ui_t *ui)
{
    settings_sanitize(ui->set);
    if (ui->env.settings_changed) {
        ui->env.settings_changed(ui->env.ctx);
    }
}

static void listen(ui_t *ui, bool levels, bool raw)
{
    if (ui->env.listen) {
        ui->env.listen(ui->env.ctx, levels, raw);
    }
}

static void go_main(ui_t *ui)
{
    ui->depth = 0;
    ui->screen = UI_MAIN;
    listen(ui, false, false);
}

static void open_menu(ui_t *ui, uint8_t id)
{
    if (ui->depth < UI_MENU_DEPTH) {
        ui->menu[ui->depth].id = id;
        ui->menu[ui->depth].sel = 0;
        ui->menu[ui->depth].top = 0;
        ui->depth++;
    }
    ui->screen = UI_MENU;
}

static void menu_back(ui_t *ui)
{
    if (ui->depth > 0) {
        ui->depth--;
    }
    if (ui->depth == 0) {
        go_main(ui);
    }
}

uint8_t ui_suggest_sensitivity(float max_db)
{
    float target = max_db + 6.0f;
    uint8_t s = 10;
    while (s > 1 && settings_threshold_dbfs(s) < target) {
        s--;
    }
    return s;
}

void ui_init(ui_t *ui, settings_t *set, const ui_env_t *env)
{
    memset(ui, 0, sizeof(*ui));
    ui->set = set;
    ui->env = *env;
    ui->screen = UI_MAIN;
    ui->level.db = ui->level.peak = -70.0f;
}

void ui_string_done(ui_t *ui, const shot_string_t *s)
{
    if (ui->hist_n < UI_HIST_MAX) {
        ui->hist_n++;
    }
    memmove(&ui->hist[1], &ui->hist[0], sizeof(shot_string_t) * (ui->hist_n - 1));
    ui->hist[0] = *s;
    ui->cur = 0;
    ui->shot_sel = s->n ? s->n - 1 : 0;
}

void ui_level(ui_t *ui, float db, bool clipped)
{
    ui->level.db = db;
    ui->level.clipped = clipped;
    if (db > ui->level.peak) {
        ui->level.peak = db;
    }
}

void ui_raw_shot(ui_t *ui, float db, int64_t now_us)
{
    if (ui->screen == UI_LEARN && !ui->learn.done && now_us < ui->learn.end_us) {
        if (ui->learn.count == 0 || db > ui->learn.max_db) {
            ui->learn.max_db = db;
        }
        ui->learn.count++;
    }
}

void ui_tick(ui_t *ui, int64_t now_us)
{
    if (ui->screen == UI_LEARN && !ui->learn.done && now_us >= ui->learn.end_us) {
        ui->learn.done = true;
        ui->learn.suggest = ui->learn.count ? ui_suggest_sensitivity(ui->learn.max_db) : ui->set->sensitivity;
        listen(ui, false, false);
    }
    // Spitzenwert fällt langsam ab (ca. 3 dB/s bei 10 Aufrufen pro Sekunde)
    ui->level.peak -= 0.3f;
    if (ui->level.peak < ui->level.db) {
        ui->level.peak = ui->level.db;
    }
}

bool ui_animating(const ui_t *ui, const timer_core_t *snap)
{
    return snap->state != TC_READY || ui->screen == UI_QUICK || ui->screen == UI_MICTEST ||
           ui->screen == UI_LEARN;
}

// ---------- Tasten ----------

static void key_menu(ui_t *ui, const btn_event_t *ev, int64_t now_us)
{
    item_t items[MENU_MAX_ITEMS];
    int n = build_menu(ui, ui->menu[ui->depth - 1].id, items);
    uint8_t *sel = &ui->menu[ui->depth - 1].sel;
    if (*sel >= n) {
        *sel = n - 1;
    }
    switch (ev->key) {
    case BTN_UP:
        *sel = (*sel + n - 1) % n;
        break;
    case BTN_DOWN:
        *sel = (*sel + 1) % n;
        break;
    case BTN_BACK:
        if (ev->press == PRESS_LONG) {
            go_main(ui);
        } else {
            menu_back(ui);
        }
        break;
    case BTN_OK: {
        const item_t *it = &items[*sel];
        if (it->disabled) {
            break;
        }
        switch (it->type) {
        case IT_SUB:
            open_menu(ui, it->arg);
            break;
        case IT_FIELD:
            ui->edit.field = it->arg;
            ui->edit.value = field_get(ui->set, it->arg);
            ui->screen = UI_EDIT;
            break;
        case IT_MODE:
            ui->set->mode = it->arg;
            changed(ui);
            toast(ui, MODE_NAMES[it->arg], now_us);
            menu_back(ui);
            break;
        case IT_PRESET: {
            const preset_t *p = &PRESETS[it->arg];
            ui->set->mode = p->mode;
            ui->set->delay_type = p->delay_type;
            ui->set->delay_min_ms = p->delay_min_ms;
            ui->set->delay_max_ms = p->delay_max_ms;
            ui->set->par_count = 1;
            ui->set->par_ms[0] = p->par_ms;
            if (p->dry_reps) {
                ui->set->dry_reps = p->dry_reps;
            }
            changed(ui);
            toast(ui, p->name, now_us);
            go_main(ui);
            break;
        }
        case IT_ACT:
            switch (it->arg) {
            case A_HISTORY:
                ui->hist_sel = ui->cur;
                ui->screen = UI_HISTORY;
                break;
            case A_MICTEST:
                ui->screen = UI_MICTEST;
                listen(ui, true, false);
                break;
            case A_LEARN:
                memset(&ui->learn, 0, sizeof(ui->learn));
                ui->learn.end_us = now_us + LEARN_US;
                ui->screen = UI_LEARN;
                listen(ui, true, true);
                break;
            case A_BEEPTEST:
                if (ui->env.beep_test) {
                    ui->env.beep_test(ui->env.ctx);
                }
                break;
            case A_SCORE: {
                const shot_string_t *c = current(ui);
                if (c->scored) {
                    memcpy(ui->score.v, c->score, SCORE_FIELDS);
                } else {
                    memset(ui->score.v, 0, SCORE_FIELDS);
                    ui->score.v[SCORE_A] = c->n;
                }
                ui->score.idx = 0;
                ui->score.result = false;
                ui->screen = UI_SCORE;
                break;
            }
            case A_DELLAST: {
                shot_string_t *c = current(ui);
                ss_delete_last(c);
                ui->shot_sel = c->n ? c->n - 1 : 0;
                toast(ui, "Letzter Schuss gelöscht", now_us);
                break;
            }
            case A_FACTORY:
                ui->screen = UI_CONFIRM;
                break;
            }
            break;
        }
        break;
    }
    default:
        break;
    }
}

static void key_edit(ui_t *ui, const btn_event_t *ev)
{
    field_t f = field_def(ui->edit.field);
    int32_t mult = ev->repeat > 30 ? 50 : ev->repeat > 10 ? 10 : 1;
    switch (ev->key) {
    case BTN_UP:
    case BTN_DOWN: {
        int32_t d = ev->key == BTN_UP ? 1 : -1;
        if (f.opts) {
            ui->edit.value = (ui->edit.value + d + f.nopts) % f.nopts;
        } else {
            int32_t v = ui->edit.value + d * f.step * mult;
            v = (v / f.step) * f.step;
            ui->edit.value = v < f.min ? f.min : v > f.max ? f.max : v;
        }
        break;
    }
    case BTN_OK:
        field_set(ui->set, ui->edit.field, ui->edit.value);
        if (ui->edit.field == F_DELAY_MIN && ui->set->delay_max_ms < ui->set->delay_min_ms) {
            ui->set->delay_max_ms = ui->set->delay_min_ms;
        }
        if (ui->edit.field == F_DELAY_MAX && ui->set->delay_min_ms > ui->set->delay_max_ms) {
            ui->set->delay_min_ms = ui->set->delay_max_ms;
        }
        changed(ui);
        ui->screen = UI_MENU;
        break;
    case BTN_BACK:
        ui->screen = UI_MENU;
        break;
    default:
        break;
    }
}

static void key_score(ui_t *ui, const btn_event_t *ev, int64_t now_us)
{
    shot_string_t *c = current(ui);
    if (ui->score.result) {
        if (ev->key == BTN_OK) {
            memcpy(c->score, ui->score.v, SCORE_FIELDS);
            c->scored = true;
            c->power_factor = ui->set->power_factor;
            toast(ui, "Wertung gespeichert", now_us);
            go_main(ui);
        } else if (ev->key == BTN_BACK) {
            ui->score.result = false;
        }
        return;
    }
    uint8_t *v = &ui->score.v[ui->score.idx];
    switch (ev->key) {
    case BTN_UP:
        if (*v < 99) {
            (*v)++;
        }
        break;
    case BTN_DOWN:
        if (*v > 0) {
            (*v)--;
        }
        break;
    case BTN_OK:
        if (ui->score.idx < SCORE_FIELDS - 1) {
            ui->score.idx++;
        } else {
            ui->score.result = true;
        }
        break;
    case BTN_BACK:
        if (ui->score.idx > 0) {
            ui->score.idx--;
        } else {
            ui->screen = UI_MENU;
        }
        break;
    default:
        break;
    }
}

static void browse(uint8_t *sel, uint8_t n, btn_key_t key)
{
    if (n) {
        *sel = key == BTN_DOWN ? (*sel + 1) % n : (*sel + n - 1) % n;
    }
}

void ui_key(ui_t *ui, const btn_event_t *ev, const timer_core_t *snap, int64_t now_us)
{
    // Ausgeschaltet: nur START weckt
    if (ui->screen == UI_OFF) {
        if (ev->key == BTN_START && ev->press == PRESS_SHORT) {
            ui->screen = UI_MAIN;
            if (ui->env.power) {
                ui->env.power(ui->env.ctx, true);
            }
        }
        return;
    }
    // Während Verzögerung, Lauf und Pause wirkt nur START
    if (snap->state != TC_READY) {
        if (ev->key == BTN_START && ev->press == PRESS_SHORT && ui->env.start_key) {
            ui->env.start_key(ui->env.ctx);
        }
        return;
    }
    if (ev->key == BTN_START && ev->press == PRESS_LONG) {
        go_main(ui);
        ui->screen = UI_OFF;
        if (ui->env.power) {
            ui->env.power(ui->env.ctx, false);
        }
        return;
    }
    if (ev->press == PRESS_LONG && ev->key != BTN_BACK) {
        return;
    }

    switch (ui->screen) {
    case UI_MAIN:
        switch (ev->key) {
        case BTN_START:
            if (ui->env.start_key) {
                ui->env.start_key(ui->env.ctx);
            }
            break;
        case BTN_UP:
        case BTN_DOWN:
            if (current(ui)) {
                browse(&ui->shot_sel, current(ui)->n, ev->key);
            }
            break;
        case BTN_OK:
            open_menu(ui, M_MAIN);
            break;
        case BTN_BACK:
            if (ev->press == PRESS_SHORT) {
                ui->screen = UI_QUICK;
                listen(ui, true, false);
            }
            break;
        default:
            break;
        }
        break;

    case UI_MENU:
        key_menu(ui, ev, now_us);
        break;

    case UI_EDIT:
        key_edit(ui, ev);
        break;

    case UI_QUICK:
    case UI_MICTEST:
        if (ev->key == BTN_UP || ev->key == BTN_DOWN) {
            int s = ui->set->sensitivity + (ev->key == BTN_UP ? 1 : -1);
            ui->set->sensitivity = s < 1 ? 1 : s > 10 ? 10 : s;
            changed(ui);
        } else if (ev->key == BTN_OK || ev->key == BTN_BACK) {
            listen(ui, false, false);
            ui->screen = ui->screen == UI_QUICK ? UI_MAIN : UI_MENU;
        } else if (ev->key == BTN_START && ui->screen == UI_QUICK) {
            go_main(ui);
            if (ui->env.start_key) {
                ui->env.start_key(ui->env.ctx);
            }
        }
        break;

    case UI_LEARN:
        if (ev->key == BTN_BACK) {
            listen(ui, false, false);
            ui->screen = UI_MENU;
        } else if (ev->key == BTN_OK && ui->learn.done) {
            if (ui->learn.count) {
                ui->set->sensitivity = ui->learn.suggest;
                changed(ui);
                toast(ui, "Empfindlichkeit übernommen", now_us);
            }
            ui->screen = UI_MENU;
        }
        break;

    case UI_HISTORY:
        if (ev->key == BTN_UP || ev->key == BTN_DOWN) {
            browse(&ui->hist_sel, ui->hist_n, ev->key);
        } else if (ev->key == BTN_OK && ui->hist_n) {
            const shot_string_t *s = &ui->hist[ui->hist_sel];
            ui->hview_sel = s->n ? s->n - 1 : 0;
            ui->screen = UI_HVIEW;
        } else if (ev->key == BTN_BACK) {
            ui->screen = ev->press == PRESS_LONG ? UI_MAIN : UI_MENU;
            if (ev->press == PRESS_LONG) {
                go_main(ui);
            }
        }
        break;

    case UI_HVIEW:
        if (ev->key == BTN_UP || ev->key == BTN_DOWN) {
            browse(&ui->hview_sel, ui->hist[ui->hist_sel].n, ev->key);
        } else if (ev->key == BTN_OK) {
            ui->cur = ui->hist_sel;
            ui->shot_sel = ui->hview_sel;
            go_main(ui);
        } else if (ev->key == BTN_BACK) {
            ui->screen = UI_HISTORY;
        }
        break;

    case UI_SCORE:
        key_score(ui, ev, now_us);
        break;

    case UI_CONFIRM:
        if (ev->key == BTN_OK) {
            settings_defaults(ui->set);
            changed(ui);
            toast(ui, "Werkseinstellungen geladen", now_us);
            go_main(ui);
        } else if (ev->key == BTN_BACK) {
            ui->screen = UI_MENU;
        }
        break;

    case UI_OFF:
        break;
    }
}

// ---------- Zeichnen ----------

static void status_bar(ui_t *ui, gfx_t *g, const char *title, gfx_color_t title_color)
{
    gfx_rect(g, 0, 0, GFX_W, 20, C_PANEL);
    gfx_text(g, F_SMALL, 8, 16, GFX_LEFT, title_color, title);
    // Akku-Symbol (Füllstand folgt mit der Akkumessung, M6)
    gfx_rect(g, 290, 5, 22, 11, C_FG);
    gfx_rect(g, 291, 6, 20, 9, C_PANEL);
    gfx_rect(g, 312, 8, 2, 5, C_FG);
    gfx_rect(g, 292, 7, 15, 7, C_FG);
    gfx_textf(g, F_SMALL, 282, 16, GFX_RIGHT, C_FG, "S%u", ui->set->sensitivity);
}

static void footer(gfx_t *g, const char *text)
{
    gfx_rect(g, 0, 222, GFX_W, 18, C_PANEL);
    gfx_text(g, F_LABEL, GFX_W / 2, 236, GFX_CENTER, C_MUTED, text);
}

static void fmt_t(char *buf, uint32_t us)
{
    ss_format_time(buf, us);
}

static void draw_string(ui_t *ui, gfx_t *g, const shot_string_t *s, uint8_t idx, const char *title, const char *foot)
{
    status_bar(ui, g, title, C_ACC);
    char a[8], b[8];
    if (s == NULL || s->n == 0) {
        gfx_text(g, F_HUGE, GFX_W / 2, 118, GFX_CENTER, C_FG, "0.00");
        gfx_text(g, F_TEXT, GFX_W / 2, 156, GFX_CENTER, C_MUTED, s ? "Keine Schüsse erkannt" : "Bereit – START drücken");
        footer(g, foot);
        return;
    }
    if (idx >= s->n) {
        idx = s->n - 1;
    }
    gfx_textf(g, F_LABEL, 10, 38, GFX_LEFT, C_MUTED, "SCHUSS %u/%u", idx + 1, s->n);
    if (s->par_count) {
        fmt_t(a, s->par_ms[s->par_count - 1] * 1000u);
        gfx_textf(g, F_LABEL, 310, 38, GFX_RIGHT, C_MUTED, "PAR %s", a);
    }
    fmt_t(a, s->shots[idx].t_us);
    gfx_text(g, F_HUGE, GFX_W / 2, 114, GFX_CENTER, ss_is_late(s, idx) ? C_BAD : C_FG, a);
    fmt_t(a, ss_split_us(s, idx));
    gfx_textf(g, F_TEXT, 10, 142, GFX_LEFT, C_FG, "Split %s", a);
    fmt_t(a, ss_first_us(s));
    gfx_textf(g, F_TEXT, 310, 142, GFX_RIGHT, C_FG, "1. Schuss %s", a);
    gfx_rect(g, 10, 152, 300, 1, C_DIM);

    const char *labels[3] = { "GESAMT", "Ø SPLIT", s->scored ? "HIT FACTOR" : "SCHNELLSTER" };
    char vals[3][12];
    fmt_t(vals[0], ss_total_us(s));
    if (s->n > 1) {
        fmt_t(vals[1], ss_avg_split_us(s));
        fmt_t(b, ss_fastest_split_us(s));
    } else {
        snprintf(vals[1], sizeof(vals[1]), "–");
        snprintf(b, sizeof(b), "–");
    }
    if (s->scored) {
        snprintf(vals[2], sizeof(vals[2]), "%.3f", (double)ss_hit_factor(s));
    } else {
        snprintf(vals[2], sizeof(vals[2]), "%s", b);
    }
    for (int i = 0; i < 3; i++) {
        int x = 10 + i * 105;
        gfx_text(g, F_LABEL, x, 172, GFX_LEFT, C_MUTED, labels[i]);
        gfx_text(g, F_VALUE, x, 202, GFX_LEFT, (i == 2 && s->scored) ? C_ACC : C_FG, vals[i]);
    }
    footer(g, foot);
}

static void draw_menu(ui_t *ui, gfx_t *g)
{
    uint8_t id = ui->menu[ui->depth - 1].id;
    uint8_t *sel = &ui->menu[ui->depth - 1].sel, *top = &ui->menu[ui->depth - 1].top;
    item_t items[MENU_MAX_ITEMS];
    int n = build_menu(ui, id, items);
    const int rows = 7, rh = 28, y0 = 22;
    if (*sel >= n) {
        *sel = n - 1;
    }
    if (*sel < *top) {
        *top = *sel;
    }
    if (*sel >= *top + rows) {
        *top = *sel - rows + 1;
    }
    status_bar(ui, g, MENU_TITLES[id], C_FG);
    for (int k = 0; k < rows && *top + k < n; k++) {
        int i = *top + k, y = y0 + k * rh;
        const item_t *it = &items[i];
        bool on = i == *sel;
        if (on) {
            gfx_rect(g, 0, y, 314, rh, C_ACC);
        }
        gfx_color_t col = on ? C_ON_ACC : it->disabled ? C_DIM : C_FG;
        char label[GFX_TEXT_MAX], val[GFX_TEXT_MAX];
        snprintf(label, sizeof(label), "%s%s", (it->type == IT_MODE && ui->set->mode == it->arg) ? "● " : "", it->label);
        gfx_text(g, F_TEXT, 10, y + 21, GFX_LEFT, col, label);
        item_value(ui, it, val, sizeof(val));
        if (it->type == IT_SUB || it->type == IT_ACT || it->type == IT_PRESET) {
            size_t l = strlen(val);
            snprintf(val + l, sizeof(val) - l, "%s›", l ? "  " : "");
        }
        gfx_text(g, F_SMALL, 306, y + 20, GFX_RIGHT, on ? C_ON_ACC : C_MUTED, val);
    }
    if (n > rows) {
        int h = 196 * rows / n;
        gfx_rect(g, 316, y0 + 196 * *top / n, 3, h, C_DIM);
    }
    footer(g, "▲▼ Auswahl   OK Öffnen   ◄ Zurück");
}

static void draw_edit(ui_t *ui, gfx_t *g)
{
    draw_menu(ui, g);
    gfx_dim(g, 0, 20, GFX_W, 202, 200);
    gfx_rect(g, 24, 44, 272, 150, C_PANEL);
    gfx_rect(g, 24, 44, 272, 3, C_ACC);
    item_t items[MENU_MAX_ITEMS];
    int n = build_menu(ui, ui->menu[ui->depth - 1].id, items);
    const char *label = "";
    for (int i = 0; i < n; i++) {
        if (items[i].type == IT_FIELD && items[i].arg == ui->edit.field) {
            label = items[i].label;
        }
    }
    gfx_text(g, F_TEXT, GFX_W / 2, 78, GFX_CENTER, C_MUTED, label);
    char v[GFX_TEXT_MAX];
    field_format(ui->edit.field, ui->edit.value, v, sizeof(v));
    gfx_text(g, F_LARGE, GFX_W / 2, 140, GFX_CENTER, C_FG, v);
    field_t f = field_def(ui->edit.field);
    if (!f.opts) {
        char lo[16], hi[16];
        field_format(ui->edit.field, f.min, lo, sizeof(lo));
        field_format(ui->edit.field, f.max, hi, sizeof(hi));
        gfx_textf(g, F_LABEL, GFX_W / 2, 178, GFX_CENTER, C_MUTED, "%s … %s", lo, hi);
    }
    footer(g, "▲▼ Ändern   OK Übernehmen   ◄ Verwerfen");
}

static void level_meter(ui_t *ui, gfx_t *g, int y, bool show_thr)
{
    const int x0 = 20, w = 280;
#define TO_X(db) (x0 + (int)(w * ((db) < -60 ? 0.0f : (db) > 0 ? 1.0f : ((db) + 60.0f) / 60.0f)))
    float thr = settings_threshold_dbfs(ui->set->sensitivity);
    gfx_rect(g, x0, y, w, 16, C_DIM);
    gfx_rect(g, x0, y, TO_X(ui->level.db) - x0, 16, ui->level.db >= thr ? C_ACC : C_GOOD);
    gfx_rect(g, TO_X(ui->level.peak) - 1, y - 2, 2, 20, C_FG);
    if (show_thr) {
        gfx_rect(g, TO_X(thr) - 1, y - 6, 3, 28, C_BAD);
        gfx_textf(g, F_LABEL, TO_X(thr), y + 38, GFX_CENTER, C_MUTED, "Schwelle %.0f dBFS", (double)thr);
    }
    static const char *const ticks[] = { "-60", "-40", "-20", "0" };
    for (int i = 0; i < 4; i++) {
        gfx_text(g, F_LABEL, x0 + i * w / 3, y - 6, GFX_CENTER, C_MUTED, ticks[i]);
    }
#undef TO_X
}

static void draw_quick(ui_t *ui, gfx_t *g, const char *title, const char *foot)
{
    status_bar(ui, g, title, C_FG);
    gfx_text(g, F_TEXT, GFX_W / 2, 46, GFX_CENTER, C_MUTED, "Empfindlichkeit");
    gfx_textf(g, F_HUGE, GFX_W / 2, 116, GFX_CENTER, C_FG, "%u", ui->set->sensitivity);
    for (int i = 0; i < 10; i++) {
        gfx_rect(g, 40 + i * 25, 126, 21, 8, i < ui->set->sensitivity ? C_ACC : C_DIM);
    }
    level_meter(ui, g, 164, true);
    footer(g, foot);
}

static void draw_learn(ui_t *ui, gfx_t *g, int64_t now_us)
{
    status_bar(ui, g, "Umgebung lernen", C_FG);
    if (!ui->learn.done) {
        int64_t left = ui->learn.end_us - now_us;
        gfx_text(g, F_TEXT, GFX_W / 2, 60, GFX_CENTER, C_FG, "Nachbarn schießen lassen");
        gfx_textf(g, F_HUGE, GFX_W / 2, 132, GFX_CENTER, C_ACC, "%ld s", (long)((left > 0 ? left : 0) + 999999) / 1000000);
        gfx_textf(g, F_SMALL, GFX_W / 2, 160, GFX_CENTER, C_MUTED, "%u Schüsse gemessen", ui->learn.count);
        level_meter(ui, g, 196, false);
        footer(g, "◄ Abbrechen");
        return;
    }
    if (!ui->learn.count) {
        gfx_text(g, F_TEXT, GFX_W / 2, 100, GFX_CENTER, C_FG, "Keine Schüsse gemessen");
        gfx_text(g, F_SMALL, GFX_W / 2, 130, GFX_CENTER, C_MUTED, "Empfindlichkeit unverändert");
        footer(g, "OK / ◄ Zurück");
        return;
    }
    gfx_textf(g, F_SMALL, GFX_W / 2, 52, GFX_CENTER, C_MUTED, "Lautester Nachbar %.1f dBFS", (double)ui->learn.max_db);
    gfx_text(g, F_TEXT, GFX_W / 2, 84, GFX_CENTER, C_FG, "Vorschlag");
    gfx_textf(g, F_HUGE, GFX_W / 2, 164, GFX_CENTER, C_ACC, "%u", ui->learn.suggest);
    gfx_textf(g, F_SMALL, GFX_W / 2, 196, GFX_CENTER, C_MUTED, "Schwelle %.0f dBFS",
              (double)settings_threshold_dbfs(ui->learn.suggest));
    footer(g, "OK Übernehmen   ◄ Verwerfen");
}

static void draw_history(ui_t *ui, gfx_t *g)
{
    status_bar(ui, g, "Historie", C_FG);
    if (!ui->hist_n) {
        gfx_text(g, F_TEXT, GFX_W / 2, 120, GFX_CENTER, C_MUTED, "Noch keine Strings");
        footer(g, "◄ Zurück");
        return;
    }
    const int rows = 7, rh = 28;
    int top = ui->hist_sel - 3;
    if (top > ui->hist_n - rows) {
        top = ui->hist_n - rows;
    }
    if (top < 0) {
        top = 0;
    }
    for (int k = 0; k < rows && top + k < ui->hist_n; k++) {
        int i = top + k, y = 22 + k * rh;
        const shot_string_t *s = &ui->hist[i];
        bool on = i == ui->hist_sel;
        gfx_color_t col = on ? C_ON_ACC : C_FG, mut = on ? C_ON_ACC : C_MUTED;
        if (on) {
            gfx_rect(g, 0, y, GFX_W, rh, C_ACC);
        }
        char t[8];
        gfx_textf(g, F_TEXT, 10, y + 21, GFX_LEFT, col, "#%lu", (unsigned long)s->seq);
        gfx_text(g, F_SMALL, 64, y + 20, GFX_LEFT, mut, MODE_NAMES[s->mode]);
        gfx_textf(g, F_SMALL, 210, y + 20, GFX_RIGHT, mut, "%u Sch.", s->n);
        if (s->n) {
            fmt_t(t, ss_total_us(s));
        } else {
            snprintf(t, sizeof(t), "–");
        }
        gfx_text(g, F_TEXT, 306, y + 21, GFX_RIGHT, col, t);
    }
    footer(g, "▲▼ Auswahl   OK Anzeigen   ◄ Zurück");
}

static void draw_score(ui_t *ui, gfx_t *g)
{
    const shot_string_t *c = current(ui);
    char title[24];
    snprintf(title, sizeof(title), "Wertung #%lu", (unsigned long)c->seq);
    status_bar(ui, g, title, C_FG);
    shot_string_t tmp = *c;
    memcpy(tmp.score, ui->score.v, SCORE_FIELDS);
    tmp.scored = true;
    tmp.power_factor = ui->set->power_factor;
    int pts = ss_points(ui->score.v, ui->set->power_factor);
    if (ui->score.result) {
        char t[8];
        fmt_t(t, ss_total_us(c));
        gfx_textf(g, F_LABEL, 20, 48, GFX_LEFT, C_MUTED, "PUNKTE (%s)", OPT_PF[ui->set->power_factor]);
        gfx_textf(g, F_VALUE, 20, 82, GFX_LEFT, C_FG, "%d", pts);
        gfx_text(g, F_LABEL, 180, 48, GFX_LEFT, C_MUTED, "ZEIT");
        gfx_text(g, F_VALUE, 180, 82, GFX_LEFT, C_FG, t);
        gfx_text(g, F_LABEL, 20, 118, GFX_LEFT, C_MUTED, "HIT FACTOR");
        gfx_textf(g, F_LARGE, 20, 176, GFX_LEFT, C_ACC, "%.4f", (double)ss_hit_factor(&tmp));
        footer(g, "OK Speichern   ◄ Korrigieren");
        return;
    }
    static const char *const names[SCORE_FIELDS] = { "A", "C", "D", "Miss", "NS" };
    for (int i = 0; i < SCORE_FIELDS; i++) {
        int x = 12 + i * 61;
        bool on = i == ui->score.idx;
        gfx_rect(g, x, 46, 54, 100, on ? C_ACC : C_PANEL);
        gfx_text(g, F_SMALL, x + 27, 70, GFX_CENTER, on ? C_ON_ACC : C_MUTED, names[i]);
        gfx_textf(g, F_LARGE, x + 27, 128, GFX_CENTER, on ? C_ON_ACC : C_FG, "%u", ui->score.v[i]);
    }
    gfx_textf(g, F_TEXT, GFX_W / 2, 186, GFX_CENTER, C_FG, "Punkte %d   ·   HF %.4f", pts, (double)ss_hit_factor(&tmp));
    footer(g, "▲▼ Anzahl   OK Weiter   ◄ Zurück");
}

static void draw_confirm(ui_t *ui, gfx_t *g)
{
    status_bar(ui, g, "Werkseinstellungen", C_FG);
    gfx_text(g, F_TEXT, GFX_W / 2, 96, GFX_CENTER, C_FG, "Alle Einstellungen zurücksetzen?");
    gfx_text(g, F_SMALL, GFX_W / 2, 128, GFX_CENTER, C_MUTED, "Die Historie bleibt erhalten.");
    footer(g, "OK Zurücksetzen   ◄ Abbrechen");
}

// Zeitleiste mit Par-Marken, Schüssen und aktueller Zeit
static void timeline(gfx_t *g, const timer_core_t *tc, int64_t t_us)
{
    const shot_string_t *r = &tc->run;
    int64_t span = t_us + 500000;
    if (r->par_count) {
        int64_t p = (int64_t)r->par_ms[r->par_count - 1] * 1250;
        span = p > span ? p : span;
    }
    if (tc->cfg.mode == MODE_COUNTDOWN && (int64_t)tc->cfg.countdown_s * 1000000 > span) {
        span = (int64_t)tc->cfg.countdown_s * 1000000;
    }
    if (span < 3000000) {
        span = 3000000;
    }
#define TX(us) (10 + (int)(300 * (us) / span))
    gfx_rect(g, 10, 206, 300, 4, C_DIM);
    for (int i = 0; i < r->par_count; i++) {
        gfx_rect(g, TX((int64_t)r->par_ms[i] * 1000) - 1, 198, 2, 20, C_GOOD);
    }
    for (int i = 0; i < r->n; i++) {
        gfx_rect(g, TX(r->shots[i].t_us) - 1, 202, 2, 12, C_FG);
    }
    gfx_rect(g, TX(t_us) - 1, 196, 3, 24, C_ACC);
#undef TX
}

static void draw_run(ui_t *ui, gfx_t *g, const timer_core_t *tc, int64_t now_us)
{
    const shot_string_t *r = &tc->run;
    uint32_t t = tc_elapsed_us(tc, now_us);
    char a[8];
    status_bar(ui, g, MODE_NAMES[tc->cfg.mode], C_ACC);
    if (tc->cfg.mode == MODE_COUNTDOWN) {
        int64_t left = (int64_t)tc->cfg.countdown_s * 1000000 - t;
        gfx_text(g, F_LABEL, GFX_W / 2, 40, GFX_CENTER, C_MUTED, "RESTZEIT");
        gfx_textf(g, F_HUGE, GFX_W / 2, 126, GFX_CENTER, C_ACC, "%.1f", (left > 0 ? left : 0) / 1e6);
        gfx_textf(g, F_TEXT, GFX_W / 2, 166, GFX_CENTER, C_FG, "%u Schuss", r->n);
    } else if (tc->cfg.mode == MODE_DRY) {
        gfx_text(g, F_LABEL, GFX_W / 2, 40, GFX_CENTER, C_MUTED, "TROCKENTRAINING");
        fmt_t(a, t);
        gfx_text(g, F_HUGE, GFX_W / 2, 126, GFX_CENTER, C_FG, a);
        gfx_textf(g, F_TEXT, GFX_W / 2, 166, GFX_CENTER, C_MUTED, "Durchgang %u/%u",
                  tc->cfg.dry_reps - tc->reps_left + 1, tc->cfg.dry_reps);
    } else {
        fmt_t(a, t);
        gfx_textf(g, F_LABEL, 10, 38, GFX_LEFT, C_MUTED, "LÄUFT %s S", a);
        if (r->n) {
            gfx_textf(g, F_LABEL, 310, 38, GFX_RIGHT, C_MUTED, "SCHUSS %u", r->n);
            fmt_t(a, r->shots[r->n - 1].t_us);
            gfx_text(g, F_HUGE, GFX_W / 2, 124, GFX_CENTER, ss_is_late(r, r->n - 1) ? C_BAD : C_FG, a);
            fmt_t(a, ss_split_us(r, r->n - 1));
            gfx_textf(g, F_VALUE, GFX_W / 2, 164, GFX_CENTER, C_FG, "Split %s", a);
        } else {
            gfx_text(g, F_LABEL, 310, 38, GFX_RIGHT, C_MUTED, "WARTE AUF SCHUSS");
            gfx_text(g, F_HUGE, GFX_W / 2, 124, GFX_CENTER, C_DIM, "-.--");
        }
    }
    timeline(g, tc, t);
}

static void draw_delay(ui_t *ui, gfx_t *g, const timer_core_t *tc, int64_t now_us)
{
    status_bar(ui, g, MODE_NAMES[tc->cfg.mode], C_ACC);
    gfx_text(g, F_LARGE, GFX_W / 2, 108, GFX_CENTER, C_FG, "Bereit");
    int k = (int)(((now_us - tc->delay_start_us) / 333333) % 4);
    for (int i = 0; i < 3; i++) {
        gfx_circle(g, 136 + i * 24, 140, 6, i < k ? C_ACC : C_DIM);
    }
    if (tc->cfg.delay_type == DELAY_FIXED) {
        int64_t left = tc->delay_end_us - now_us;
        gfx_textf(g, F_TEXT, GFX_W / 2, 186, GFX_CENTER, C_MUTED, "Start in %.1f s", (left > 0 ? left : 0) / 1e6);
    } else if (tc->cfg.delay_type == DELAY_RANDOM) {
        gfx_text(g, F_TEXT, GFX_W / 2, 186, GFX_CENTER, C_MUTED, "Zufallsstart");
    }
    footer(g, "START Abbrechen");
}

static void draw_pause(ui_t *ui, gfx_t *g, const timer_core_t *tc, int64_t now_us)
{
    status_bar(ui, g, MODE_NAMES[tc->cfg.mode], C_ACC);
    gfx_text(g, F_TEXT, GFX_W / 2, 66, GFX_CENTER, C_MUTED, "Nächster Durchgang");
    int64_t left = tc->pause_end_us - now_us;
    gfx_textf(g, F_HUGE, GFX_W / 2, 146, GFX_CENTER, C_ACC, "%ld s", (long)(((left > 0 ? left : 0) + 999999) / 1000000));
    if (tc->has_last && tc->last.n) {
        char t[8];
        fmt_t(t, ss_total_us(&tc->last));
        gfx_textf(g, F_TEXT, GFX_W / 2, 184, GFX_CENTER, C_FG, "Letzter: %u Schuss, %s s", tc->last.n, t);
    }
    footer(g, "START Serie beenden");
}

void ui_render(ui_t *ui, const timer_core_t *snap, int64_t now_us, gfx_t *g)
{
    gfx_begin(g, C_BG);
    if (ui->screen == UI_OFF) {
        return;
    }
    switch (snap->state) {
    case TC_DELAY:
        draw_delay(ui, g, snap, now_us);
        break;
    case TC_RUN:
        draw_run(ui, g, snap, now_us);
        break;
    case TC_PAUSE:
        draw_pause(ui, g, snap, now_us);
        break;
    case TC_READY:
        switch (ui->screen) {
        case UI_MAIN:
            draw_string(ui, g, current(ui), ui->shot_sel, MODE_NAMES[ui->set->mode],
                        "▲▼ Schüsse   OK Menü   ◄ Empfindl.");
            break;
        case UI_MENU:
            draw_menu(ui, g);
            break;
        case UI_EDIT:
            draw_edit(ui, g);
            break;
        case UI_QUICK:
            draw_quick(ui, g, "Schnelleinstellung", "▲▼ Ändern   OK Fertig");
            break;
        case UI_MICTEST:
            draw_quick(ui, g, "Mikrofon-Test", "▲▼ Empfindlichkeit   OK Fertig");
            break;
        case UI_LEARN:
            draw_learn(ui, g, now_us);
            break;
        case UI_HISTORY:
            draw_history(ui, g);
            break;
        case UI_HVIEW: {
            char title[24];
            snprintf(title, sizeof(title), "Historie #%lu", (unsigned long)ui->hist[ui->hist_sel].seq);
            draw_string(ui, g, &ui->hist[ui->hist_sel], ui->hview_sel, title, "▲▼ Schüsse   OK Übernehmen   ◄ Liste");
            break;
        }
        case UI_SCORE:
            draw_score(ui, g);
            break;
        case UI_CONFIRM:
            draw_confirm(ui, g);
            break;
        case UI_OFF:
            break;
        }
        break;
    }
    // Meldung oben unter der Statuszeile, damit sie keine Werte verdeckt
    if (ui->toast[0] && now_us < ui->toast_until) {
        int w = gfx_text_width(F_SMALL, ui->toast) + 28;
        gfx_rect(g, GFX_W / 2 - w / 2, 24, w, 26, C_FG);
        gfx_text(g, F_SMALL, GFX_W / 2, 43, GFX_CENTER, C_BG, ui->toast);
    }
}
