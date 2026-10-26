#include "buttons.h"

#include <stdbool.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define POLL_MS         5
#define DEBOUNCE_MS     20
#define LONG_START_MS   2000
#define LONG_BACK_MS    1000
#define REPEAT_DELAY_MS 400
#define REPEAT_MS       90

static const int s_pins[BTN_COUNT] = { PIN_KEY_START, PIN_KEY_UP, PIN_KEY_DOWN, PIN_KEY_OK, PIN_KEY_BACK };
static btn_cb_t s_cb;

typedef struct {
    bool stable;            // entprellter Zustand (true = gedrückt)
    unsigned change_ms;     // Dauer, die der Rohzustand vom stabilen abweicht
    unsigned held_ms;
    unsigned repeats;
    bool long_sent;
} btn_state_t;

static bool raw_pressed(btn_key_t k)
{
    bool p = gpio_get_level(s_pins[k]) == 0;
    if (k == BTN_START) {
        p = p || gpio_get_level(PIN_BOOT_BTN) == 0;
    }
    return p;
}

static void emit(btn_key_t k, btn_press_t p, unsigned repeat)
{
    btn_event_t ev = { .key = k, .press = p, .repeat = repeat };
    s_cb(&ev);
}

static void on_pressed(btn_key_t k, btn_state_t *st)
{
    st->held_ms = 0;
    st->repeats = 0;
    st->long_sent = false;
    if (k == BTN_UP || k == BTN_DOWN) {
        emit(k, PRESS_SHORT, 0);
    }
}

static void on_held(btn_key_t k, btn_state_t *st)
{
    st->held_ms += POLL_MS;
    unsigned long_ms = k == BTN_START ? LONG_START_MS : k == BTN_BACK ? LONG_BACK_MS : 0;
    if (long_ms && !st->long_sent && st->held_ms >= long_ms) {
        st->long_sent = true;
        emit(k, PRESS_LONG, 0);
    }
    if ((k == BTN_UP || k == BTN_DOWN) && st->held_ms >= REPEAT_DELAY_MS &&
        (st->held_ms - REPEAT_DELAY_MS) % REPEAT_MS == 0) {
        emit(k, PRESS_REPEAT, ++st->repeats);
    }
}

static void on_released(btn_key_t k, btn_state_t *st)
{
    if (k != BTN_UP && k != BTN_DOWN && !st->long_sent) {
        emit(k, PRESS_SHORT, 0);
    }
}

static void buttons_task(void *arg)
{
    btn_state_t st[BTN_COUNT] = { 0 };
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        for (int k = 0; k < BTN_COUNT; k++) {
            bool raw = raw_pressed(k);
            if (raw != st[k].stable) {
                st[k].change_ms += POLL_MS;
                if (st[k].change_ms >= DEBOUNCE_MS) {
                    st[k].stable = raw;
                    st[k].change_ms = 0;
                    if (raw) {
                        on_pressed(k, &st[k]);
                    } else {
                        on_released(k, &st[k]);
                    }
                }
            } else {
                st[k].change_ms = 0;
                if (st[k].stable) {
                    on_held(k, &st[k]);
                }
            }
        }
    }
}

void buttons_start(btn_cb_t cb)
{
    s_cb = cb;
    uint64_t mask = 1ULL << PIN_BOOT_BTN;
    for (int k = 0; k < BTN_COUNT; k++) {
        mask |= 1ULL << s_pins[k];
    }
    gpio_config_t io = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);
    xTaskCreatePinnedToCore(buttons_task, "buttons", 3072, NULL, 6, NULL, 0);
}
