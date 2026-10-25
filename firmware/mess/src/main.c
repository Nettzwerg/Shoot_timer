// Mess-Firmware für Prototyp-Phase 1 (Mikrofon) und 2 (Beep).
// Ausgabe zeilenweise über die serielle Konsole (921600 Baud), maschinenlesbar:
//   SHOT,<n>,<t_s>,<split_s>,<peak_dbfs>,<rise_ms>,<low>,<high>,<high_clipped>
//   LVL,<peak_dbfs>,<low>,<high>,<high_clipped>
//   CAP_BEGIN,<trigger_idx>,<pairs>  ...  <i>,<low>,<high>  ...  CAP_END
//   SWEEP,<freq_hz>,<peak_dbfs>
//   BEEP,<idx>
// Zeilen mit '#' sind Kommentare. Befehle siehe print_help().

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "beeper.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "envelope.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "pins.h"

#define CONSOLE_UART    UART_NUM_0
#define CMD_LINE_MAX        64

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

#define OUT(...)                                    \
    do {                                            \
        xSemaphoreTake(s_out, portMAX_DELAY);       \
        printf(__VA_ARGS__);                        \
        xSemaphoreGive(s_out);                      \
    } while (0)

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
        "#  c 0|1             Mitschnitt je Schuss (50 ms vor, 450 ms nach) aus/ein\n"
        "#  t <dBFS>          Schwelle, z. B. t -35\n"
        "#  d <ms>            Echo-/Totzeit, z. B. d 50\n"
        "#  k <faktor>        Verstaerkungsverhaeltnis HIGH/LOW, z. B. k 11\n"
        "#  b [Hz] [ms] [vol] Beep (vol 0=voll 1=halb 2=leise), Werte bleiben gespeichert\n"
        "#  w <f1> <f2> <df>  Frequenz-Sweep des Beeps mit Pegelmessung am Mikrofon\n"
        "#  g [ms]            Timer-Durchgang: Verzoegerung (Standard zufaellig 2-4 s), Beep, Schuesse\n"
        "#  x                 Timer-Durchgang beenden\n"
        "#  p 0|1             Boost-Wandler aus/ein\n"
        "#  a 0|1             Analogteil aus/ein\n");
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
}

static void beep_blanked(uint32_t hz, uint32_t ms, beep_volume_t vol)
{
    uint32_t now = env_now_idx();
    env_blank_until(now + ENV_MS_TO_SAMPLES(ms + 20));
    OUT("BEEP,%lu\n", (unsigned long)now);
    beeper_beep(hz, ms, vol);
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
    OUT("# Bereit - Start in %d ms\n", delay_ms);
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
        env_set_capture(atoi(arg) != 0);
        break;
    case 't':
        cfg->threshold_dbfs = strtof(arg, NULL);
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
        OUT("# Durchgang beendet, %lu Schuss\n", (unsigned long)s_shot_no);
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

static void print_capture(const env_event_t *e)
{
    uint32_t n, start;
    const uint16_t *d = env_capture_data(&n, &start);
    xSemaphoreTake(s_out, portMAX_DELAY);
    printf("CAP_BEGIN,%lu,%lu\n", (unsigned long)e->idx, (unsigned long)n);
    for (uint32_t k = 0; k < n; k++) {
        printf("%lu,%u,%u\n", (unsigned long)(start + k), d[k * 2], d[k * 2 + 1]);
    }
    printf("CAP_END\n");
    xSemaphoreGive(s_out);
    env_capture_release();
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
            OUT("SHOT,%lu,%.3f,%.3f,%.1f,%.2f,%u,%u,%d\n", (unsigned long)s_shot_no, t, split,
                e.peak_dbfs, e.rise_ms, e.peak_low, e.peak_high, e.high_clipped);
            break;
        }
        case ENV_EVT_LEVEL:
            OUT("LVL,%.1f,%u,%u,%d\n", e.peak_dbfs, e.peak_low, e.peak_high, e.high_clipped);
            break;
        case ENV_EVT_CAPTURE:
            print_capture(&e);
            break;
        }
    }
}

void app_main(void)
{
    s_out = xSemaphoreCreateMutex();

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_ANALOG_EN,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_ANALOG_EN, 0);

    beeper_init();
    beeper_power(true);
    env_start();

    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 512, 0, 0, NULL, 0));
    xTaskCreatePinnedToCore(output_task, "out", 4096, NULL, 5, NULL, 0);

    OUT("# Shot-Timer Mess-Firmware, 20 kS/s je Kanal\n");
    OUT("#SHOT,n,t_s,split_s,peak_dbfs,rise_ms,low,high,high_clipped\n");
    print_help();

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
