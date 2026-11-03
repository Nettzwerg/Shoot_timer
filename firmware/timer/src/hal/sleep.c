#include "sleep.h"

#include "beeper.h"
#include "board_pins.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"

// Ausgänge, deren Pegel im Tiefschlaf gehalten werden muss
static const gpio_num_t s_held[] = {
    PIN_ANALOG_EN,  // High: Analogteil aus
    PIN_BOOST_EN,   // Low: Boostwandler aus
    PIN_BUZ_SLEEP,  // Low: H-Brücke schläft
    PIN_TFT_BL,     // Low: Beleuchtung aus
};
// Nur START weckt. BOOT (GPIO 0) ist ein Strapping-Pin; ohne START-Taste weckt die RESET-Taste des DevKits.
static const gpio_num_t s_wake[] = { PIN_KEY_START };

bool sleep_after_boot(void)
{
    gpio_deep_sleep_hold_dis();
    for (size_t i = 0; i < sizeof(s_held) / sizeof(s_held[0]); i++) {
        gpio_hold_dis(s_held[i]);
    }
    for (size_t i = 0; i < sizeof(s_wake) / sizeof(s_wake[0]); i++) {
        rtc_gpio_deinit(s_wake[i]);     // wieder als normale GPIO für die Tasten
    }
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
}

static void hold_level(gpio_num_t pin, int level)
{
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, level);
    gpio_hold_en(pin);
}

void sleep_enter(void)
{
    display_power(false);
    beeper_power(false);
    hold_level(PIN_ANALOG_EN, 1);
    hold_level(PIN_BOOST_EN, 0);
    hold_level(PIN_BUZ_SLEEP, 0);
    hold_level(PIN_TFT_BL, 0);
    gpio_deep_sleep_hold_en();

    uint64_t mask = 0;
    for (size_t i = 0; i < sizeof(s_wake) / sizeof(s_wake[0]); i++) {
        mask |= 1ULL << s_wake[i];
        rtc_gpio_pullup_en(s_wake[i]);
        rtc_gpio_pulldown_dis(s_wake[i]);
    }
    // Pull-ups der Wecktasten brauchen die RTC-Peripherie auch im Schlaf
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();
}
