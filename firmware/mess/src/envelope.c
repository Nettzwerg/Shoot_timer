#include "envelope.h"

#include <math.h>
#include <string.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_continuous.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pins.h"

#define N_CH            3
#define FRAME_PAIRS     64                      // 3,2 ms je DMA-Rahmen
#define FRAME_BYTES     (FRAME_PAIRS * N_CH * SOC_ADC_DIGI_RESULT_BYTES)
#define RING_PAIRS      16384                   // 0,82 s, Zweierpotenz
#define RING_MASK       (RING_PAIRS - 1)
#define CAP_PRE         ENV_MS_TO_SAMPLES(50)
#define CAP_POST        ENV_MS_TO_SAMPLES(450)
#define CAP_PAIRS       (CAP_PRE + CAP_POST)
#define LEVEL_PERIOD    ENV_MS_TO_SAMPLES(100)
#define ADC_MAX         4095
#define HIGH_CLIP_RAW   3500                    // ca. 2,7 V: darüber gilt HIGH als übersteuert
#define BASE_ALPHA      (1.0f / 8192.0f)        // Ruhelage, ca. 0,4 s Zeitkonstante

static env_config_t s_cfg = {
    .threshold_dbfs = -45.0f,   // niedrig, damit auch Nachbarschüsse erfasst werden
    .dead_time_ms = 50,
    .ratio = 11.0f,
};

static adc_continuous_handle_t s_adc;
static adc_cali_handle_t s_cali;
static QueueHandle_t s_events;

static uint16_t s_ring[RING_PAIRS * 2];
static uint16_t s_cap[CAP_PAIRS * 2];

static volatile uint32_t s_idx;                 // nächster Sample-Index
static volatile uint32_t s_idx_at_frame;
static volatile int64_t s_frame_time_us;
static volatile uint32_t s_blank_until;
static volatile uint32_t s_overflows;
static float s_base_low = 2048.0f, s_base_high = 2048.0f;
static float s_bat_raw;
static bool s_base_valid;

// pro DMA-Rahmen aus der Konfiguration berechnet
static float s_fs;                              // Vollausschlag kombiniert
static float s_thr;                             // Schwelle kombiniert
static uint32_t s_dead_samples;

static struct {
    bool active;
    uint32_t idx, peak_idx;
    float peak_a;
    uint16_t peak_low, peak_high;
    bool clipped;
} s_evt;

static volatile bool s_level_on;
static struct {
    uint32_t count;
    float peak_a;
    uint16_t peak_low, peak_high;
    bool clipped;
} s_lvl;

static volatile bool s_cap_on;
static volatile bool s_cap_busy;
static bool s_cap_pending;
static uint32_t s_cap_start, s_cap_end;

static volatile bool s_win_active, s_win_done;
static uint32_t s_win_from, s_win_to;
static float s_win_peak;

static float to_dbfs(float a)
{
    if (a < 1.0f) {
        a = 1.0f;
    }
    return 20.0f * log10f(a / s_fs);
}

static void post(const env_event_t *e)
{
    xQueueSend(s_events, e, 0);   // bei voller Queue verwerfen, Erfassung hat Vorrang
}

static void process_pair(uint16_t low, uint16_t high)
{
    uint32_t i = s_idx;
    s_ring[(i & RING_MASK) * 2] = low;
    s_ring[(i & RING_MASK) * 2 + 1] = high;

    if (!s_base_valid) {
        s_base_low = low;
        s_base_high = high;
        s_base_valid = true;
    }

    float l = fmaxf(low - s_base_low, 0.0f);
    float h = fmaxf(high - s_base_high, 0.0f);
    bool clipped = high >= HIGH_CLIP_RAW;
    float a = clipped ? l * s_cfg.ratio : h;

    // Ruhelage nur in ruhigen Phasen nachführen
    if (!s_evt.active && a < s_thr * 0.25f) {
        s_base_low += (low - s_base_low) * BASE_ALPHA;
        s_base_high += (high - s_base_high) * BASE_ALPHA;
    }

    if (s_evt.active) {
        if (a > s_evt.peak_a) {
            s_evt.peak_a = a;
            s_evt.peak_idx = i;
            s_evt.peak_low = (uint16_t)l;
            s_evt.peak_high = (uint16_t)h;
            s_evt.clipped = clipped;
        }
        if (i - s_evt.idx >= s_dead_samples) {
            env_event_t e = {
                .type = ENV_EVT_SHOT,
                .idx = s_evt.idx,
                .peak_dbfs = to_dbfs(s_evt.peak_a),
                .rise_ms = (s_evt.peak_idx - s_evt.idx) * (ENV_SAMPLE_US / 1000.0f),
                .peak_low = s_evt.peak_low,
                .peak_high = s_evt.peak_high,
                .high_clipped = s_evt.clipped,
            };
            post(&e);
            s_evt.active = false;
        }
    } else if (a >= s_thr && (int32_t)(i - s_blank_until) >= 0) {
        s_evt.active = true;
        s_evt.idx = i;
        s_evt.peak_idx = i;
        s_evt.peak_a = a;
        s_evt.peak_low = (uint16_t)l;
        s_evt.peak_high = (uint16_t)h;
        s_evt.clipped = clipped;
        if (s_cap_on && !s_cap_busy && !s_cap_pending) {
            s_cap_pending = true;
            s_cap_start = i - CAP_PRE;
            s_cap_end = i + CAP_POST;
        }
    }

    if (s_cap_pending && i + 1 == s_cap_end) {
        for (uint32_t k = 0; k < CAP_PAIRS; k++) {
            uint32_t r = (s_cap_start + k) & RING_MASK;
            s_cap[k * 2] = s_ring[r * 2];
            s_cap[k * 2 + 1] = s_ring[r * 2 + 1];
        }
        s_cap_pending = false;
        s_cap_busy = true;
        env_event_t e = { .type = ENV_EVT_CAPTURE, .idx = s_cap_start + CAP_PRE };
        post(&e);
    }

    if (s_level_on) {
        if (a > s_lvl.peak_a) {
            s_lvl.peak_a = a;
            s_lvl.clipped = clipped;
        }
        if (l > s_lvl.peak_low) {
            s_lvl.peak_low = (uint16_t)l;
        }
        if (h > s_lvl.peak_high) {
            s_lvl.peak_high = (uint16_t)h;
        }
        if (++s_lvl.count >= LEVEL_PERIOD) {
            env_event_t e = {
                .type = ENV_EVT_LEVEL,
                .idx = i,
                .peak_dbfs = to_dbfs(s_lvl.peak_a),
                .peak_low = s_lvl.peak_low,
                .peak_high = s_lvl.peak_high,
                .high_clipped = s_lvl.clipped,
            };
            post(&e);
            memset(&s_lvl, 0, sizeof(s_lvl));
        }
    }

    if (s_win_active && (int32_t)(i - s_win_from) >= 0) {
        if (a > s_win_peak) {
            s_win_peak = a;
        }
        if ((int32_t)(i - s_win_to) >= 0) {
            s_win_active = false;
            s_win_done = true;
        }
    }

    s_idx = i + 1;
}

static bool IRAM_ATTR on_pool_ovf(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata,
                                  void *user_data)
{
    s_overflows++;
    return false;
}

static void adc_task(void *arg)
{
    static uint8_t buf[FRAME_BYTES];
    uint16_t last_low = 0;
    bool have_low = false;

    while (true) {
        uint32_t n = 0;
        if (adc_continuous_read(s_adc, buf, FRAME_BYTES, &n, portMAX_DELAY) != ESP_OK) {
            continue;
        }

        s_fs = (ADC_MAX - s_base_low) * s_cfg.ratio;
        s_thr = s_fs * powf(10.0f, s_cfg.threshold_dbfs / 20.0f);
        s_dead_samples = ENV_MS_TO_SAMPLES(s_cfg.dead_time_ms);

        for (uint32_t k = 0; k + SOC_ADC_DIGI_RESULT_BYTES <= n; k += SOC_ADC_DIGI_RESULT_BYTES) {
            const adc_digi_output_data_t *p = (const adc_digi_output_data_t *)&buf[k];
            uint32_t ch = p->type2.channel;
            uint16_t v = p->type2.data;
            if (ch == ADC_CH_LOW) {
                last_low = v;
                have_low = true;
            } else if (ch == ADC_CH_HIGH && have_low) {
                process_pair(last_low, v);
                have_low = false;
            } else if (ch == ADC_CH_VBAT) {
                s_bat_raw += (v - s_bat_raw) * 0.01f;
            }
        }

        s_frame_time_us = esp_timer_get_time();
        s_idx_at_frame = s_idx;
    }
}

void env_start(void)
{
    s_events = xQueueCreate(32, sizeof(env_event_t));

    adc_continuous_handle_cfg_t hcfg = {
        .max_store_buf_size = FRAME_BYTES * 64,   // ca. 200 ms Puffer, überbrückt Flash-Schreibzugriffe
        .conv_frame_size = FRAME_BYTES,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&hcfg, &s_adc));

    adc_digi_pattern_config_t pattern[N_CH];
    const adc_channel_t chans[N_CH] = { ADC_CH_LOW, ADC_CH_HIGH, ADC_CH_VBAT };
    for (int k = 0; k < N_CH; k++) {
        pattern[k] = (adc_digi_pattern_config_t) {
            .atten = ADC_ATTEN_DB_12,
            .channel = chans[k],
            .unit = ADC_UNIT_1,
            .bit_width = SOC_ADC_DIGI_MAX_BITWIDTH,
        };
    }
    adc_continuous_config_t dcfg = {
        .pattern_num = N_CH,
        .adc_pattern = pattern,
        .sample_freq_hz = ENV_SAMPLE_RATE_HZ * N_CH,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_ERROR_CHECK(adc_continuous_config(s_adc, &dcfg));

    adc_continuous_evt_cbs_t cbs = { .on_pool_ovf = on_pool_ovf };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(s_adc, &cbs, NULL));

    adc_cali_curve_fitting_config_t ccfg = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CH_VBAT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_curve_fitting(&ccfg, &s_cali) != ESP_OK) {
        s_cali = NULL;
    }

    ESP_ERROR_CHECK(adc_continuous_start(s_adc));
    xTaskCreatePinnedToCore(adc_task, "adc", 4096, NULL, configMAX_PRIORITIES - 2, NULL, 1);
}

env_config_t *env_config(void)
{
    return &s_cfg;
}

bool env_get_event(env_event_t *evt, uint32_t timeout_ms)
{
    return xQueueReceive(s_events, evt, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

uint32_t env_now_idx(void)
{
    uint32_t idx = s_idx_at_frame;
    int64_t dt = esp_timer_get_time() - s_frame_time_us;
    return idx + (uint32_t)(dt / ENV_SAMPLE_US);
}

void env_blank_until(uint32_t idx)
{
    s_blank_until = idx;
}

void env_set_level_report(bool on)
{
    s_level_on = on;
}

void env_set_capture(bool on)
{
    s_cap_on = on;
}

const uint16_t *env_capture_data(uint32_t *n_pairs, uint32_t *start_idx)
{
    *n_pairs = CAP_PAIRS;
    *start_idx = s_cap_start;
    return s_cap;
}

void env_capture_release(void)
{
    s_cap_busy = false;
}

void env_window_start(uint32_t from_idx, uint32_t to_idx)
{
    s_win_done = false;
    s_win_peak = 0.0f;
    s_win_from = from_idx;
    s_win_to = to_idx;
    s_win_active = true;
}

bool env_window_result(float *peak_dbfs)
{
    if (!s_win_done) {
        return false;
    }
    *peak_dbfs = to_dbfs(s_win_peak);
    return true;
}

float env_battery_v(void)
{
    int mv = 0;
    if (s_cali == NULL || adc_cali_raw_to_voltage(s_cali, (int)s_bat_raw, &mv) != ESP_OK) {
        mv = (int)(s_bat_raw * 3100.0f / ADC_MAX);
    }
    return mv * 2.0f / 1000.0f;   // Teiler 1:2
}

void env_status(float *base_low, float *base_high, uint32_t *overflows)
{
    *base_low = s_base_low;
    *base_high = s_base_high;
    *overflows = s_overflows;
}
