#include "beeper.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "board_pins.h"

#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_RES        LEDC_TIMER_10_BIT
#define DUTY_MAX        (1u << 10)
#define CH_IN1          LEDC_CHANNEL_0
#define CH_IN2          LEDC_CHANNEL_1
#define BOOST_SETTLE_MS 30

static bool s_powered;
static esp_timer_handle_t s_stop_timer;

static void set_duty(ledc_channel_t ch, uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, ch, duty);
    ledc_update_duty(LEDC_MODE, ch);
}

void beeper_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_BUZ_SLEEP) | (1ULL << PIN_BOOST_EN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(PIN_BUZ_SLEEP, 0);
    gpio_set_level(PIN_BOOST_EN, 0);

    ledc_timer_config_t t = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = 3000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));

    // IN2 invertiert: bei gleichem Tastgrad gegenphasig zu IN1 (Vollbrücke)
    ledc_channel_config_t c1 = {
        .gpio_num = PIN_BUZ_IN1,
        .speed_mode = LEDC_MODE,
        .channel = CH_IN1,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
    };
    ledc_channel_config_t c2 = c1;
    c2.gpio_num = PIN_BUZ_IN2;
    c2.channel = CH_IN2;
    c2.flags.output_invert = 1;
    ESP_ERROR_CHECK(ledc_channel_config(&c1));
    ESP_ERROR_CHECK(ledc_channel_config(&c2));
}

void beeper_power(bool on)
{
    if (on && !s_powered) {
        gpio_set_level(PIN_BOOST_EN, 1);
        vTaskDelay(pdMS_TO_TICKS(BOOST_SETTLE_MS));
    } else if (!on) {
        beeper_stop();
        gpio_set_level(PIN_BOOST_EN, 0);
    }
    s_powered = on;
}

bool beeper_powered(void)
{
    return s_powered;
}

void beeper_start(uint32_t freq_hz, beep_volume_t vol)
{
    beeper_power(true);
    ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq_hz);

    // DRV8833: IN2 dauerhaft High (invertiert, Tastgrad 0) -> OUT wechselt zwischen 0 und -VM
    switch (vol) {
    case BEEP_VOL_FULL:
        set_duty(CH_IN1, DUTY_MAX / 2);
        set_duty(CH_IN2, DUTY_MAX / 2);
        break;
    case BEEP_VOL_HALF:
        set_duty(CH_IN1, DUTY_MAX / 2);
        set_duty(CH_IN2, 0);
        break;
    case BEEP_VOL_LOW:
    default:
        set_duty(CH_IN1, DUTY_MAX / 10);
        set_duty(CH_IN2, 0);
        break;
    }
    gpio_set_level(PIN_BUZ_SLEEP, 1);
}

void beeper_stop(void)
{
    gpio_set_level(PIN_BUZ_SLEEP, 0);   // Ausgänge hochohmig
    set_duty(CH_IN1, 0);
    set_duty(CH_IN2, 0);
}

static void stop_cb(void *arg)
{
    beeper_stop();
}

void beeper_beep_async(uint32_t freq_hz, uint32_t ms, beep_volume_t vol)
{
    if (s_stop_timer == NULL) {
        const esp_timer_create_args_t args = { .callback = stop_cb, .name = "beep_stop" };
        ESP_ERROR_CHECK(esp_timer_create(&args, &s_stop_timer));
    }
    esp_timer_stop(s_stop_timer);   // laufenden Ton verlängern statt abbrechen
    beeper_start(freq_hz, vol);
    esp_timer_start_once(s_stop_timer, (uint64_t)ms * 1000);
}

void beeper_beep(uint32_t freq_hz, uint32_t ms, beep_volume_t vol)
{
    beeper_start(freq_hz, vol);
    vTaskDelay(pdMS_TO_TICKS(ms));
    beeper_stop();
}
