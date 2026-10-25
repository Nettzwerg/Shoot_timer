// Mess-Firmware für Prototyp-Phase 1 (Mikrofon) und 2 (Beep).
// Ausgabe zeilenweise über die serielle Konsole (921600 Baud), maschinenlesbar:
//   SHOT,<n>,<t_s>,<split_s>,<peak_dbfs>,<rise_ms>,<low>,<high>,<high_clipped>
//   LVL,<peak_dbfs>,<low>,<high>,<high_clipped>
//   CAP_BEGIN,<trigger_idx>,<pairs>  ...  <i>,<low>,<high>  ...  CAP_END
//   SWEEP,<freq_hz>,<peak_dbfs>
//   BEEP,<idx>
//   MARK,Situation <n>
// Zeilen mit '#' sind Kommentare. Befehle siehe print_help().
//
// Standalone-Betrieb (ohne Laptop, z. B. an einer Powerbank):
//   SHOT-, BEEP- und MARK-Zeilen sowie die ersten 3 Mitschnitte nach jeder
//   Markierung werden im Flash gespeichert (Befehl F gibt alles aus).
//   Taste BOOT kurz:  nächste Situation markieren, Quittung per Beep
//                     (je 5 ein langer, je 1 ein kurzer Ton)
//   Taste BOOT lang:  Timer-Durchgang (wie Befehl g)
//
// Smartphone (auch iPhone): WLAN "ShotTimer-Mess" (Passwort shoot1234), im Browser
// http://192.168.4.1 öffnen. Live-Schussliste, Markieren, Timer-Durchgang, Download.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beeper.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "envelope.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "pins.h"
#include "storage.h"
#include "web.h"

#define CONSOLE_UART    UART_NUM_0
#define CMD_LINE_MAX    64
#define CAPS_PER_MARK   3
#define LONG_PRESS_MS   1500

static SemaphoreHandle_t s_out;

static struct {
    bool events_on;
    uint32_t beep_hz;
    uint32_t beep_ms;
    beep_volume_t beep_vol;
} s_set = {
    .events_on = true,
    .beep_hz = 3000,
    .beep_ms = 300,
    .beep_vol = BEEP_VOL_FULL,
};

// Laufender Timer-Durchgang (Befehl g)
static volatile bool s_run;
static volatile uint32_t s_run_beep_idx;
static uint32_t s_shot_no;
static uint32_t s_last_shot_idx;

static volatile bool s_cap_serial;          // Mitschnitte über die Konsole ausgeben (Befehl c)
static volatile uint32_t s_cap_budget;      // noch zu speichernde Mitschnitte
static uint32_t s_situation;               // bleibt über Neustarts erhalten (NVS), bis X
static uint32_t s_wifi = 1;                 // WLAN-Weboberfläche, Änderung wirkt nach Neustart
static bool s_wifi_ok;
static QueueHandle_t s_cmd_queue;           // Befehle aus der Weboberfläche

static void nvs_load(const char *key, uint32_t *val)
{
    nvs_handle_t h;
    if (nvs_open("mess", NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u32(h, key, val);
        nvs_close(h);
    }
}

static void nvs_save(const char *key, uint32_t val)
{
    nvs_handle_t h;
    if (nvs_open("mess", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, key, val);
        nvs_commit(h);
        nvs_close(h);
    }
}

#define OUT(...)                                    \
    do {                                            \
        xSemaphoreTake(s_out, portMAX_DELAY);       \
        printf(__VA_ARGS__);                        \
        xSemaphoreGive(s_out);                      \
    } while (0)

// Zeile auf Konsole und Weboberfläche ausgeben, optional im Flash speichern
static void publish(bool store, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void publish(bool store, const char *fmt, ...)
{
    char line[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    OUT("%s\n", line);
    if (store) {
        store_line("%s", line);
    }
    web_publish(line);
}

static void update_info(void)
{
    char info[96];
    snprintf(info, sizeof(info), "Situation %lu \xc2\xb7 Schwelle %.0f dBFS \xc2\xb7 Sitzung %lu%s",
             (unsigned long)s_situation, env_config()->threshold_dbfs, (unsigned long)store_session(),
             store_ok() ? "" : " \xc2\xb7 SPEICHERFEHLER");
    web_set_info(info);
}

static float idx_to_s(uint32_t idx)
{
    return idx * (ENV_SAMPLE_US / 1e6f);
}

static void print_help(void)
{
    OUT("# Befehle:\n"
        "#  h                 Hilfe\n"
        "#  s                 Status\n"
        "#  e 0|1             Schussmeldungen aus/ein\n"
        "#  l 0|1             Pegelmonitor (10 Hz) aus/ein\n"
        "#  c 0|1             Mitschnitt je Schuss (50 ms vor, 300 ms nach) aus/ein\n"
        "#  t <dBFS>          Schwelle, z. B. t -35\n"
        "#  d <ms>            Echo-/Totzeit, z. B. d 50\n"
        "#  k <faktor>        Verstaerkungsverhaeltnis HIGH/LOW, z. B. k 11\n"
        "#  b [Hz] [ms] [vol] Beep (vol 0=voll 1=halb 2=leise), Werte bleiben gespeichert\n"
        "#  w <f1> <f2> <df>  Frequenz-Sweep des Beeps mit Pegelmessung am Mikrofon\n"
        "#  g [ms]            Timer-Durchgang: Verzoegerung (Standard zufaellig 2-4 s), Beep, Schuesse\n"
        "#  x                 Timer-Durchgang beenden\n"
        "#  p 0|1             Boost-Wandler aus/ein\n"
        "#  a 0|1             Analogteil aus/ein\n"
        "#  m                 naechste Situation markieren (wie Taste BOOT kurz)\n"
        "#  i                 Speicherbelegung\n"
        "#  F                 alle gespeicherten Sitzungen ausgeben (fuer mess_logger.py download)\n"
        "#  X                 alle gespeicherten Sitzungen loeschen\n"
        "#  n 0|1             WLAN-Weboberflaeche aus/ein (wirkt nach Neustart)\n");
}

static void print_status(void)
{
    env_config_t *cfg = env_config();
    float bl, bh;
    uint32_t ovf;
    env_status(&bl, &bh, &ovf);
    OUT("# Schwelle %.1f dBFS, Totzeit %lu ms, Verhaeltnis %.2f\n"
        "# Ruhelage LOW %.0f, HIGH %.0f, DMA-Ueberlaeufe %lu\n"
        "# Beep %lu Hz, %lu ms, Lautstaerke %d, Boost %s\n"
        "# Akku %.2f V (ohne Akku bedeutungslos)\n",
        cfg->threshold_dbfs, (unsigned long)cfg->dead_time_ms, cfg->ratio,
        bl, bh, (unsigned long)ovf,
        (unsigned long)s_set.beep_hz, (unsigned long)s_set.beep_ms, s_set.beep_vol,
        beeper_powered() ? "ein" : "aus", env_battery_v());
    OUT("# WLAN %s, freier RAM %lu Byte\n", s_wifi_ok ? "aktiv (ShotTimer-Mess, http://192.168.4.1)" : "aus",
        (unsigned long)esp_get_free_heap_size());
    xSemaphoreTake(s_out, portMAX_DELAY);
    store_info();
    xSemaphoreGive(s_out);
}

static void beep_blanked(uint32_t hz, uint32_t ms, beep_volume_t vol)
{
    uint32_t now = env_now_idx();
    env_blank_until(now + ENV_MS_TO_SAMPLES(ms + 20));
    publish(true, "BEEP,%lu", (unsigned long)now);
    beeper_beep(hz, ms, vol);
}

static void update_capture(void)
{
    env_set_capture(s_cap_serial || s_cap_budget > 0);
}

// Quittungston für die Situationsnummer: je 5 ein langer, je 1 ein kurzer Ton
static void beep_number(uint32_t n)
{
    uint32_t now = env_now_idx();
    env_blank_until(now + ENV_MS_TO_SAMPLES((n / 5) * 800 + (n % 5) * 350 + 100));
    for (uint32_t k = 0; k < n / 5; k++) {
        beeper_beep(s_set.beep_hz, 500, BEEP_VOL_LOW);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    for (uint32_t k = 0; k < n % 5; k++) {
        beeper_beep(s_set.beep_hz, 100, BEEP_VOL_LOW);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

static void cmd_mark(void)
{
    s_situation++;
    nvs_save("sit", s_situation);
    s_run = false;
    publish(true, "MARK,Situation %lu", (unsigned long)s_situation);
    update_info();
    s_cap_budget = CAPS_PER_MARK;
    update_capture();
    beep_number(s_situation);
}

static void cmd_sweep(uint32_t f1, uint32_t f2, uint32_t df)
{
    if (df == 0 || f2 < f1) {
        OUT("# Fehler: w <f1> <f2> <df>\n");
        return;
    }
    for (uint32_t f = f1; f <= f2; f += df) {
        uint32_t now = env_now_idx();
        env_blank_until(now + ENV_MS_TO_SAMPLES(260));
        env_window_start(now + ENV_MS_TO_SAMPLES(60), now + ENV_MS_TO_SAMPLES(180));
        beeper_beep(f, 200, BEEP_VOL_FULL);
        float db = 0.0f;
        while (!env_window_result(&db)) {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        OUT("SWEEP,%lu,%.1f\n", (unsigned long)f, db);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void cmd_go(int delay_ms)
{
    if (delay_ms < 0) {
        delay_ms = 2000 + (int)(esp_random() % 2001);
    }
    s_run = false;
    publish(false, "# Bereit - Start in %.1f s", delay_ms / 1000.0f);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    s_shot_no = 0;
    s_run_beep_idx = env_now_idx();
    s_last_shot_idx = s_run_beep_idx;
    s_run = true;
    beep_blanked(s_set.beep_hz, s_set.beep_ms, s_set.beep_vol);
}

static void handle_line(char *line)
{
    char c = line[0];
    char *arg = line + 1;
    env_config_t *cfg = env_config();

    switch (c) {
    case 'h':
        print_help();
        break;
    case 's':
        print_status();
        break;
    case 'e':
        s_set.events_on = atoi(arg) != 0;
        break;
    case 'l':
        env_set_level_report(atoi(arg) != 0);
        break;
    case 'c':
        s_cap_serial = atoi(arg) != 0;
        update_capture();
        break;
    case 'm':
        cmd_mark();
        break;
    case 'i':
        xSemaphoreTake(s_out, portMAX_DELAY);
        store_info();
        xSemaphoreGive(s_out);
        break;
    case 'F':
        xSemaphoreTake(s_out, portMAX_DELAY);
        store_dump();
        xSemaphoreGive(s_out);
        break;
    case 'X':
        store_erase();
        s_situation = 0;
        nvs_save("sit", s_situation);
        publish(false, "# Speicher geloescht, neue Sitzung %lu", (unsigned long)store_session());
        update_info();
        break;
    case 'n':
        s_wifi = atoi(arg) != 0;
        nvs_save("wifi", s_wifi);
        OUT("# WLAN %s nach Neustart\n", s_wifi ? "ein" : "aus");
        break;
    case 't':
        cfg->threshold_dbfs = strtof(arg, NULL);
        update_info();
        break;
    case 'd':
        cfg->dead_time_ms = (uint32_t)atoi(arg);
        break;
    case 'k':
        cfg->ratio = strtof(arg, NULL);
        break;
    case 'b': {
        unsigned hz = s_set.beep_hz, ms = s_set.beep_ms, vol = s_set.beep_vol;
        sscanf(arg, "%u %u %u", &hz, &ms, &vol);
        s_set.beep_hz = hz;
        s_set.beep_ms = ms;
        s_set.beep_vol = vol > 2 ? BEEP_VOL_LOW : (beep_volume_t)vol;
        beep_blanked(s_set.beep_hz, s_set.beep_ms, s_set.beep_vol);
        break;
    }
    case 'w': {
        unsigned f1 = 0, f2 = 0, df = 0;
        sscanf(arg, "%u %u %u", &f1, &f2, &df);
        cmd_sweep(f1, f2, df);
        break;
    }
    case 'g': {
        int ms = -1;
        sscanf(arg, "%d", &ms);
        cmd_go(ms);
        break;
    }
    case 'x':
        s_run = false;
        publish(false, "# Durchgang beendet, %lu Schuss", (unsigned long)s_shot_no);
        break;
    case 'p':
        beeper_power(atoi(arg) != 0);
        break;
    case 'a':
        gpio_set_level(PIN_ANALOG_EN, atoi(arg) != 0 ? 0 : 1);
        break;
    case '\0':
        break;
    default:
        OUT("# Unbekannter Befehl, h fuer Hilfe\n");
        break;
    }
}

static void handle_capture(const env_event_t *e)
{
    uint32_t n, start;
    const uint16_t *d = env_capture_data(&n, &start);
    if (s_cap_budget > 0) {
        store_capture(e->idx, start, d, n);
        s_cap_budget--;
    }
    if (s_cap_serial) {
        xSemaphoreTake(s_out, portMAX_DELAY);
        printf("CAP_BEGIN,%lu,%lu\n", (unsigned long)e->idx, (unsigned long)n);
        for (uint32_t k = 0; k < n; k++) {
            printf("%lu,%u,%u\n", (unsigned long)(start + k), d[k * 2], d[k * 2 + 1]);
        }
        printf("CAP_END\n");
        xSemaphoreGive(s_out);
    }
    update_capture();
    env_capture_release();
}

// Befehle aus der Weboberfläche laufen in einer eigenen Task, weil z. B. g mehrere Sekunden blockiert
static void web_cmd(const char *line)
{
    char buf[CMD_LINE_MAX];
    strlcpy(buf, line, sizeof(buf));
    xQueueSend(s_cmd_queue, buf, 0);
}

static void cmd_task(void *arg)
{
    char line[CMD_LINE_MAX];
    while (true) {
        if (xQueueReceive(s_cmd_queue, line, portMAX_DELAY) == pdTRUE) {
            handle_line(line);
        }
    }
}

static void button_task(void *arg)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_BOOT_BTN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);

    uint32_t pressed_ms = 0;
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (gpio_get_level(PIN_BOOT_BTN) == 0) {
            pressed_ms += 10;
            if (pressed_ms == LONG_PRESS_MS) {
                cmd_go(-1);
            }
        } else {
            if (pressed_ms >= 30 && pressed_ms < LONG_PRESS_MS) {
                cmd_mark();
            }
            pressed_ms = 0;
        }
    }
}

static void output_task(void *arg)
{
    env_event_t e;
    while (true) {
        if (!env_get_event(&e, 100)) {
            continue;
        }
        switch (e.type) {
        case ENV_EVT_SHOT: {
            if (!s_set.events_on) {
                break;
            }
            // Im Timer-Durchgang Zeit relativ zum Beep, sonst seit dem Einschalten
            s_shot_no++;
            float t = idx_to_s(s_run ? e.idx - s_run_beep_idx : e.idx);
            float split = (s_run || s_shot_no > 1) ? idx_to_s(e.idx - s_last_shot_idx) : 0.0f;
            s_last_shot_idx = e.idx;
            publish(true, "SHOT,%lu,%.3f,%.3f,%.1f,%.2f,%u,%u,%d", (unsigned long)s_shot_no, t, split,
                    e.peak_dbfs, e.rise_ms, e.peak_low, e.peak_high, e.high_clipped);
            break;
        }
        case ENV_EVT_LEVEL: {
            char line[64];
            snprintf(line, sizeof(line), "LVL,%.1f,%u,%u,%d", e.peak_dbfs, e.peak_low, e.peak_high,
                     e.high_clipped);
            OUT("%s\n", line);
            web_set_level(line);
            break;
        }
        case ENV_EVT_CAPTURE:
            handle_capture(&e);
            break;
        }
    }
}

void app_main(void)
{
    s_out = xSemaphoreCreateMutex();

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    nvs_load("sit", &s_situation);
    nvs_load("wifi", &s_wifi);

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_ANALOG_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_ANALOG_EN, 0);

    beeper_init();
    beeper_power(true);
    bool stored = store_init();
    env_start();

    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 512, 0, 0, NULL, 0));
    xTaskCreatePinnedToCore(output_task, "out", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(button_task, "btn", 4096, NULL, 4, NULL, 0);
    s_cmd_queue = xQueueCreate(4, CMD_LINE_MAX);
    xTaskCreatePinnedToCore(cmd_task, "cmd", 4096, NULL, 4, NULL, 0);
    if (s_wifi) {
        s_wifi_ok = web_start(web_cmd);
    }
    update_info();

    OUT("# Shot-Timer Mess-Firmware, 20 kS/s je Kanal\n");
    OUT("#SHOT,n,t_s,split_s,peak_dbfs,rise_ms,low,high,high_clipped\n");
    store_line("# Sitzung %lu, letzte Situation %lu", (unsigned long)store_session(),
               (unsigned long)s_situation);
    print_help();
    print_status();

    // Bereitschaft: 2 kurze Töne = Speicher ok, 1 langer Ton = Speicherfehler
    vTaskDelay(pdMS_TO_TICKS(500));
    if (stored) {
        beep_number(2);
    } else {
        beep_blanked(s_set.beep_hz, 1500, BEEP_VOL_LOW);
    }

    char line[CMD_LINE_MAX];
    size_t len = 0;
    while (true) {
        uint8_t ch;
        if (uart_read_bytes(CONSOLE_UART, &ch, 1, portMAX_DELAY) != 1) {
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            line[len] = '\0';
            if (len > 0) {
                handle_line(line);
            }
            len = 0;
        } else if (len < CMD_LINE_MAX - 1) {
            line[len++] = (char)ch;
        }
    }
}
