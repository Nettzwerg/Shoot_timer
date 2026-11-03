// Timer-Firmware, Meilenstein M3: Kern + Tasten + Schusserkennung + Beep + Display mit Menü.
// Tasks: timer (Zustandsautomat, Tasten, Schüsse, Konsole), ui (zeichnet das Display), buttons, shots,
// console. Kern, Einstellungen und UI-Zustand sind durch s_lock geschützt.
// Die serielle Konsole (115200 Baud) bleibt zum Testen und Protokollieren erhalten:
//   STATE,<Zustand>
//   SHOT,<n>,<zeit_s>,<split_s>,<pegel_dbfs>
//   STRING,<nr>,<modus>,<schuesse>,<erster_s>,<gesamt_s>,<schnellster_split_s>,<avg_split_s>,<ende>
// Befehle: h (Hilfe). START: Taste an GPIO 14 oder BOOT des DevKits.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beeper.h"
#include "board_pins.h"
#include "buttons.h"
#include "config_store.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "envelope.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "history.h"
#include "history_flash.h"
#include "nvs_flash.h"
#include "settings.h"
#include "shot_string.h"
#include "timer_core.h"
#include "ui.h"

#define CONSOLE_UART    UART_NUM_0
#define CMD_LEN         64
#define BEEP_WARN_MS    150
#define BEEP_END_MS     700
#define BLANK_EXTRA_MS  20      // Erkennung nach dem Beep noch so lange aus
#define LEARN_THR_DBFS  -55.0f  // Schwelle für „Umgebung lernen“: auch leise Nachbarschüsse erfassen
#define UI_FRAME_MS     40      // Bildwechsel bei laufender Anzeige
#define UI_IDLE_MS      500
#define UI_TICK_US      100000

typedef enum { EV_KEY, EV_SHOT, EV_LEVEL, EV_CMD } app_ev_type_t;

typedef struct {
    app_ev_type_t type;
    union {
        btn_event_t key;
        struct {
            int64_t t_us;
            int8_t db;
        } shot;
        struct {
            float db;
            bool clipped;
        } level;
        char cmd[CMD_LEN];
    };
} app_ev_t;

static const char *const MODE_NAMES[MODE_COUNT] = {
    "Standard", "Par", "Spy", "Trockentraining", "Wiederholung", "Countdown",
};
static const char *const STATE_NAMES[] = { "Bereit", "Verzoegerung", "Lauf", "Pause" };
static const char *const END_NAMES[] = { "-", "START", "Auto-Stopp", "Countdown", "Trocken", "Maximalzeit", "voll" };

static QueueHandle_t s_queue;
static timer_core_t s_tc;
static settings_t s_set;
static tc_state_t s_shown_state = TC_READY;

static SemaphoreHandle_t s_lock;            // schützt s_tc, s_set, s_ui und s_hist
static history_t s_hist;
static ui_t s_ui;
static gfx_t s_gfx;                         // nur im UI-Task benutzt
static TaskHandle_t s_ui_task;
static volatile bool s_listen_raw;          // Rohdetektionen an die UI („Umgebung lernen“)
static volatile int8_t s_power_req = -1;    // Anzeige ein/aus, wird im UI-Task ausgeführt

static uint32_t rand32(void)
{
    return esp_random();
}

static beep_volume_t volume(void)
{
    static const beep_volume_t map[3] = { BEEP_VOL_LOW, BEEP_VOL_HALF, BEEP_VOL_FULL };
    return map[s_set.beep_volume > 2 ? 2 : s_set.beep_volume];
}

// ---------- Ereignisquellen ----------

static void on_button(const btn_event_t *ev)
{
    app_ev_t e = { .type = EV_KEY, .key = *ev };
    xQueueSend(s_queue, &e, 0);
}

static void shot_task(void *arg)
{
    env_event_t e;
    while (true) {
        if (!env_get_event(&e, portMAX_DELAY)) {
            continue;
        }
        if (e.type == ENV_EVT_SHOT) {
            app_ev_t a = { .type = EV_SHOT };
            a.shot.t_us = env_idx_to_us(e.idx);
            a.shot.db = e.peak_dbfs < -127.0f ? -127 : (int8_t)e.peak_dbfs;
            xQueueSend(s_queue, &a, 0);
        } else if (e.type == ENV_EVT_LEVEL) {
            app_ev_t a = { .type = EV_LEVEL };
            a.level.db = e.peak_dbfs;
            a.level.clipped = e.high_clipped;
            xQueueSend(s_queue, &a, 0);
        }
    }
}

static void console_task(void *arg)
{
    app_ev_t e = { .type = EV_CMD };
    size_t len = 0;
    while (true) {
        uint8_t ch;
        if (uart_read_bytes(CONSOLE_UART, &ch, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (len > 0) {
                e.cmd[len] = '\0';
                xQueueSend(s_queue, &e, portMAX_DELAY);
            }
            len = 0;
        } else if (len < CMD_LEN - 1) {
            e.cmd[len++] = (char)ch;
        }
    }
}

// ---------- Ausgabe ----------

static void print_shot(void)
{
    const shot_string_t *r = &s_tc.run;
    char t[8], sp[8];
    ss_format_time(t, r->shots[r->n - 1].t_us);
    ss_format_time(sp, ss_split_us(r, r->n - 1));
    printf("SHOT,%u,%s,%s,%d\n", r->n, t, sp, r->shots[r->n - 1].db);
}

static void print_string(void)
{
    const shot_string_t *s = &s_tc.last;
    char first[8], total[8], best[8], avg[8];
    ss_format_time(first, ss_first_us(s));
    ss_format_time(total, ss_total_us(s));
    ss_format_time(best, ss_fastest_split_us(s));
    ss_format_time(avg, ss_avg_split_us(s));
    printf("STRING,%lu,%s,%u,%s,%s,%s,%s,%s\n", (unsigned long)s->seq, MODE_NAMES[s->mode], s->n, first,
           total, best, avg, END_NAMES[s_tc.end_reason]);
}

static void print_settings(void)
{
    printf("# Modus %s, Verzoegerung %s %u-%u ms / fest %u ms, Empfindlichkeit %u (%.0f dBFS), "
           "Echo %u ms, Auto-Stopp %u s\n",
           MODE_NAMES[s_set.mode],
           s_set.delay_type == DELAY_RANDOM ? "Zufall" : s_set.delay_type == DELAY_FIXED ? "fest" : "sofort",
           s_set.delay_min_ms, s_set.delay_max_ms, s_set.delay_fixed_ms, s_set.sensitivity,
           settings_threshold_dbfs(s_set.sensitivity), s_set.echo_ms, s_set.auto_stop_s);
    printf("# Par:");
    for (int i = 0; i < s_set.par_count; i++) {
        printf(" %.2f", s_set.par_ms[i] / 1000.0);
    }
    printf(" s | Countdown %u s, Warnung %u s | Pause %u s | Trocken %u x | Beep %u Hz %u ms Stufe %u\n",
           s_set.countdown_s, s_set.countdown_warn_s, s_set.repeat_pause_s, s_set.dry_reps, s_set.beep_hz,
           s_set.beep_ms, s_set.beep_volume);
}

static void print_help(void)
{
    printf("# Befehle (Aenderungen werden sofort gespeichert):\n"
           "#  h                Hilfe          s   Einstellungen anzeigen\n"
           "#  g                wie START      l   letzten String anzeigen\n"
           "#  m <0-5>          Modus: 0 Standard 1 Par 2 Spy 3 Trockentraining 4 Wiederholung 5 Countdown\n"
           "#  e <1-10>         Empfindlichkeit\n"
           "#  d r <von> <bis>  Zufallsstart in s   d f <s>  fester Start   d i  sofort\n"
           "#  p <t1> [t2 ...]  Par-Zeiten in s (max. 10)\n"
           "#  a <s>            Auto-Stopp (0 = aus)   o <ms>  Echo-Zeit\n"
           "#  c <s> [warn_s]   Countdown              r <s>   Pause Wiederholung\n"
           "#  n <anzahl>       Trockentraining-Durchgaenge\n"
           "#  b <Hz> <ms> <0-2> Beep (0 leise, 2 laut)\n"
           "#  w                Werkseinstellungen\n"
           "#  H                die letzten 10 Strings der Historie\n");
}

// ---------- Kern-Aktionen ausführen ----------

static void beep(uint32_t ms, bool is_start)
{
    env_blank_until(env_now_idx() + ENV_MS_TO_SAMPLES(ms + BLANK_EXTRA_MS));
    int64_t t = esp_timer_get_time();
    beeper_beep_async(s_set.beep_hz, ms, volume());
    if (is_start) {
        tc_set_t0(&s_tc, t);
    }
}

static void apply(uint32_t act)
{
    if (act & TC_ACT_DETECT_ON) {
        env_config_t *c = env_config();
        c->threshold_dbfs = settings_threshold_dbfs(s_tc.cfg.sensitivity);
        c->dead_time_ms = s_tc.cfg.echo_ms;
    }
    if (act & TC_ACT_BEEP_END) {
        beep(BEEP_END_MS, false);
    } else if (act & TC_ACT_BEEP_START) {
        beep(s_tc.cfg.beep_ms, true);
    } else if (act & TC_ACT_BEEP_PAR) {
        beep(s_tc.cfg.beep_ms, false);
    } else if (act & TC_ACT_BEEP_WARN) {
        beep(BEEP_WARN_MS, false);
    }
    if (act & TC_ACT_SHOT_ADDED) {
        print_shot();
    }
    if (act & TC_ACT_STRING_DONE) {
        print_string();
        ui_string_done(&s_ui, &s_tc.last);
    }
    if (s_tc.state != s_shown_state) {
        s_shown_state = s_tc.state;
        printf("STATE,%s\n", STATE_NAMES[s_tc.state]);
    }
}

// ---------- Befehle ----------

static void settings_changed(void)
{
    settings_sanitize(&s_set);
    config_save(&s_set);
    display_backlight(s_set.brightness);
    print_settings();
}

// ---------- Rückrufe der UI (laufen im Timer-Task, s_lock ist gehalten) ----------

static void ui_cb_start(void *ctx)
{
    apply(tc_start_key(&s_tc, &s_set, esp_timer_get_time()));
}

static void ui_cb_settings(void *ctx)
{
    settings_changed();
}

static void ui_cb_power(void *ctx, bool on)
{
    s_power_req = on;
    printf("# Anzeige %s\n", on ? "ein" : "aus");
}

static void ui_cb_beep_test(void *ctx)
{
    beep(s_set.beep_ms, false);
}

static void ui_cb_preset_save(void *ctx, uint8_t slot, const settings_t *preset)
{
    if (!preset_store(slot, preset)) {
        printf("# Fehler: Preset %u nicht gespeichert\n", slot + 1);
    }
}

static void ui_cb_listen(void *ctx, bool levels, bool raw)
{
    env_set_level_report(levels);
    s_listen_raw = raw;
    env_config_t *c = env_config();
    c->threshold_dbfs = raw ? LEARN_THR_DBFS : settings_threshold_dbfs(s_set.sensitivity);
    c->dead_time_ms = s_set.echo_ms;
}

static void handle_cmd(char *line)
{
    char *arg = line + 1;
    switch (line[0]) {
    case 'h':
        print_help();
        return;
    case 's':
        print_settings();
        return;
    case 'g':
        apply(tc_start_key(&s_tc, &s_set, esp_timer_get_time()));
        return;
    case 'l':
        if (s_tc.has_last) {
            print_string();
        }
        return;
    case 'H': {
        static shot_string_t s;
        printf("# Historie: %u Strings\n", s_hist.count);
        for (uint16_t i = 0; i < 10 && hist_get(&s_hist, i, &s); i++) {
            char total[8];
            ss_format_time(total, ss_total_us(&s));
            printf("HIST,%lu,%s,%u,%s%s\n", (unsigned long)s.seq, MODE_NAMES[s.mode], s.n, total,
                   s.scored ? ",gewertet" : "");
        }
        return;
    }
    case 'm':
        s_set.mode = (uint8_t)atoi(arg);
        break;
    case 'e':
        s_set.sensitivity = (uint8_t)atoi(arg);
        break;
    case 'd': {
        char kind = 0;
        float a = 0, b = 0;
        int n = sscanf(arg, " %c %f %f", &kind, &a, &b);
        if (kind == 'r' && n == 3) {
            s_set.delay_type = DELAY_RANDOM;
            s_set.delay_min_ms = (uint16_t)(a * 1000);
            s_set.delay_max_ms = (uint16_t)(b * 1000);
        } else if (kind == 'f' && n == 2) {
            s_set.delay_type = DELAY_FIXED;
            s_set.delay_fixed_ms = (uint16_t)(a * 1000);
        } else if (kind == 'i') {
            s_set.delay_type = DELAY_INSTANT;
        } else {
            printf("# Fehler: d r <von> <bis> | d f <s> | d i\n");
            return;
        }
        break;
    }
    case 'p': {
        uint8_t n = 0;
        char *p = arg;
        while (n < PAR_MAX) {
            char *end;
            float v = strtof(p, &end);
            if (end == p) {
                break;
            }
            s_set.par_ms[n++] = (uint32_t)(v * 1000 + 0.5f);
            p = end;
        }
        if (n == 0) {
            printf("# Fehler: p <t1> [t2 ...]\n");
            return;
        }
        s_set.par_count = n;
        break;
    }
    case 'a':
        s_set.auto_stop_s = (uint8_t)atoi(arg);
        break;
    case 'o':
        s_set.echo_ms = (uint16_t)atoi(arg);
        break;
    case 'c': {
        unsigned t = s_set.countdown_s, w = s_set.countdown_warn_s;
        sscanf(arg, "%u %u", &t, &w);
        s_set.countdown_s = (uint16_t)t;
        s_set.countdown_warn_s = (uint8_t)w;
        break;
    }
    case 'r':
        s_set.repeat_pause_s = (uint8_t)atoi(arg);
        break;
    case 'n':
        s_set.dry_reps = (uint8_t)atoi(arg);
        break;
    case 'b': {
        unsigned hz = s_set.beep_hz, ms = s_set.beep_ms, vol = s_set.beep_volume;
        sscanf(arg, "%u %u %u", &hz, &ms, &vol);
        s_set.beep_hz = (uint16_t)hz;
        s_set.beep_ms = (uint16_t)ms;
        s_set.beep_volume = (uint8_t)vol;
        break;
    }
    case 'w':
        settings_defaults(&s_set);
        break;
    default:
        printf("# Unbekannter Befehl, h fuer Hilfe\n");
        return;
    }
    if (s_tc.state != TC_READY) {
        printf("# Hinweis: gilt ab dem naechsten String\n");
    }
    settings_changed();
}

// ---------- Timer-Task ----------


static void timer_task(void *arg)
{
    app_ev_t e;
    while (true) {
        TickType_t wait = portMAX_DELAY;
        int64_t deadline = tc_next_deadline(&s_tc);
        if (deadline != INT64_MAX) {
            int64_t d = deadline - esp_timer_get_time();
            wait = d <= 0 ? 0 : pdMS_TO_TICKS((d + 999) / 1000);
        }
        bool got = xQueueReceive(s_queue, &e, wait) == pdTRUE;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        int64_t now = esp_timer_get_time();
        if (got) {
            switch (e.type) {
            case EV_KEY:
                ui_key(&s_ui, &e.key, &s_tc, now);
                break;
            case EV_SHOT:
                if (s_listen_raw) {
                    ui_raw_shot(&s_ui, e.shot.db, now);
                } else {
                    apply(tc_shot(&s_tc, e.shot.t_us, e.shot.db));
                }
                break;
            case EV_LEVEL:
                ui_level(&s_ui, e.level.db, e.level.clipped);
                break;
            case EV_CMD:
                handle_cmd(e.cmd);
                break;
            }
        }
        apply(tc_tick(&s_tc, esp_timer_get_time()));
        xSemaphoreGive(s_lock);
        xTaskNotifyGive(s_ui_task);
    }
}

// Ersatz, falls die Partition fehlt: nichts lesen, nichts speichern
static bool no_read(void *ctx, uint16_t slot, uint8_t *buf)
{
    return false;
}

static bool no_write(void *ctx, uint16_t slot, const uint8_t *buf)
{
    return false;
}

static bool no_erase(void *ctx)
{
    return false;
}

// ---------- UI-Task ----------

static void ui_task(void *arg)
{
    int64_t last_tick = 0;
    bool animating = false;
    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(animating ? UI_FRAME_MS : UI_IDLE_MS));
        if (s_power_req >= 0) {
            display_power(s_power_req);
            s_power_req = -1;
        }
        xSemaphoreTake(s_lock, portMAX_DELAY);
        int64_t now = esp_timer_get_time();
        if (now - last_tick >= UI_TICK_US) {
            ui_tick(&s_ui, now);
            last_tick = now;
        }
        ui_render(&s_ui, &s_tc, now, &s_gfx);
        animating = ui_animating(&s_ui, &s_tc);
        xSemaphoreGive(s_lock);
        display_draw(&s_gfx);   // dauert ca. 30 ms, läuft außerhalb der Sperre
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    config_load(&s_set);

    gpio_config_t io = { .pin_bit_mask = 1ULL << PIN_ANALOG_EN, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    gpio_set_level(PIN_ANALOG_EN, 0);   // Analogteil an

    beeper_init();
    beeper_power(true);
    env_start();

    s_queue = xQueueCreate(16, sizeof(app_ev_t));
    s_lock = xSemaphoreCreateMutex();

    hist_io_t hist_io;
    if (!history_flash_io(&hist_io)) {
        printf("# Fehler: Partition \"history\" fehlt, Historie wird nicht gespeichert\n");
        hist_io = (hist_io_t) { .read = no_read, .write = no_write, .erase_all = no_erase };
    }
    hist_open(&s_hist, &hist_io);
    tc_init(&s_tc, rand32, hist_next_seq(&s_hist));
    const ui_env_t env = {
        .start_key = ui_cb_start,
        .settings_changed = ui_cb_settings,
        .power = ui_cb_power,
        .beep_test = ui_cb_beep_test,
        .listen = ui_cb_listen,
        .preset_save = ui_cb_preset_save,
    };
    ui_init(&s_ui, &s_set, &s_hist, &env);
    for (uint8_t slot = 0; slot < PRESET_SLOTS; slot++) {
        settings_t p;
        if (preset_load(slot, &p)) {
            ui_set_preset(&s_ui, slot, &p);
        }
    }
    display_init();
    display_backlight(s_set.brightness);

    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0));
    printf("# Shot-Timer Firmware %s\n", UI_FW_VERSION);
    print_help();
    print_settings();
    printf("# Historie: %u Strings, naechste Nummer %lu\n", s_hist.count, (unsigned long)hist_next_seq(&s_hist));
    printf("STATE,%s\n", STATE_NAMES[s_tc.state]);

    xTaskCreatePinnedToCore(ui_task, "ui", 6144, NULL, 4, &s_ui_task, 0);
    xTaskCreatePinnedToCore(timer_task, "timer", 6144, NULL, configMAX_PRIORITIES - 3, NULL, 0);
    xTaskCreatePinnedToCore(shot_task, "shots", 3072, NULL, configMAX_PRIORITIES - 4, NULL, 0);
    xTaskCreatePinnedToCore(console_task, "console", 3072, NULL, 3, NULL, 0);
    buttons_start(on_button);
}
