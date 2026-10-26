// Timer-Firmware, Meilenstein M2: Kern + Tasten + Schusserkennung + Beep.
// Bis zum Display (M3) erfolgen Anzeige und Einstellungen über die serielle Konsole (115200 Baud):
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
#include "driver/gpio.h"
#include "driver/uart.h"
#include "envelope.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "settings.h"
#include "shot_string.h"
#include "timer_core.h"

#define CONSOLE_UART    UART_NUM_0
#define CMD_LEN         64
#define BEEP_WARN_MS    150
#define BEEP_END_MS     700
#define BLANK_EXTRA_MS  20      // Erkennung nach dem Beep noch so lange aus

typedef enum { EV_KEY, EV_SHOT, EV_CMD } app_ev_type_t;

typedef struct {
    app_ev_type_t type;
    union {
        btn_event_t key;
        struct {
            int64_t t_us;
            int8_t db;
        } shot;
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
        if (env_get_event(&e, portMAX_DELAY) && e.type == ENV_EVT_SHOT) {
            app_ev_t a = { .type = EV_SHOT };
            a.shot.t_us = env_idx_to_us(e.idx);
            a.shot.db = e.peak_dbfs < -127.0f ? -127 : (int8_t)e.peak_dbfs;
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
           "#  w                Werkseinstellungen\n");
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
    print_settings();
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

static void handle_key(const btn_event_t *k, int64_t now)
{
    // M2: nur START wirkt; Menü und Anzeige folgen mit dem Display (M3)
    if (k->key == BTN_START && k->press == PRESS_SHORT) {
        apply(tc_start_key(&s_tc, &s_set, now));
    }
}

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
        if (xQueueReceive(s_queue, &e, wait) == pdTRUE) {
            int64_t now = esp_timer_get_time();
            switch (e.type) {
            case EV_KEY:
                handle_key(&e.key, now);
                break;
            case EV_SHOT:
                apply(tc_shot(&s_tc, e.shot.t_us, e.shot.db));
                break;
            case EV_CMD:
                handle_cmd(e.cmd);
                break;
            }
        }
        apply(tc_tick(&s_tc, esp_timer_get_time()));
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
    tc_init(&s_tc, rand32, 1);

    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0));
    printf("# Shot-Timer Firmware (M2, Anzeige ueber Konsole)\n");
    print_help();
    print_settings();
    printf("STATE,%s\n", STATE_NAMES[s_tc.state]);

    xTaskCreatePinnedToCore(timer_task, "timer", 4096, NULL, configMAX_PRIORITIES - 3, NULL, 0);
    xTaskCreatePinnedToCore(shot_task, "shots", 3072, NULL, configMAX_PRIORITIES - 4, NULL, 0);
    xTaskCreatePinnedToCore(console_task, "console", 3072, NULL, 3, NULL, 0);
    buttons_start(on_button);
}
