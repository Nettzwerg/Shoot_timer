#include "display.h"

#include "board_pins.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Ausrichtung: Je nach Modul kann das Bild gespiegelt erscheinen. Dann hier anpassen.
#define LCD_SWAP_XY     true
#define LCD_MIRROR_X    true
#define LCD_MIRROR_Y    false
#define LCD_INVERT      true        // IPS-Panels mit ST7789 brauchen meist invertierte Farben
#define LCD_PCLK_HZ     (40 * 1000 * 1000)
#define LCD_HOST        SPI2_HOST

#define STRIP_H         40
#define N_BUF           2

#define BL_TIMER        LEDC_TIMER_1    // Timer 0 nutzt der Beeper
#define BL_CHANNEL      LEDC_CHANNEL_2
#define BL_RES          LEDC_TIMER_10_BIT

static esp_lcd_panel_handle_t s_panel;
static uint16_t *s_buf[N_BUF];
static SemaphoreHandle_t s_free;            // zählt freie Streifenpuffer
static uint8_t s_brightness = 100;
static bool s_on = true;

static bool on_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_free, &woken);
    return woken == pdTRUE;
}

static void backlight_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BL_RES,
        .timer_num = BL_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t c = {
        .gpio_num = PIN_TFT_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BL_CHANNEL,
        .timer_sel = BL_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
}

static void backlight_apply(void)
{
    uint32_t duty = s_on ? (uint32_t)s_brightness * ((1u << 10) - 1) / 100 : 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_CHANNEL);
}

void display_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = PIN_TFT_SCK,
        .mosi_io_num = PIN_TFT_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GFX_W * STRIP_H * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    s_free = xSemaphoreCreateCounting(N_BUF, N_BUF);
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_TFT_DC,
        .cs_gpio_num = PIN_TFT_CS,
        .pclk_hz = LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = N_BUF + 2,
        .on_color_trans_done = on_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_TFT_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &panel_cfg, &s_panel));
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_invert_color(s_panel, LCD_INVERT);
    esp_lcd_panel_swap_xy(s_panel, LCD_SWAP_XY);
    esp_lcd_panel_mirror(s_panel, LCD_MIRROR_X, LCD_MIRROR_Y);
    esp_lcd_panel_disp_on_off(s_panel, true);

    for (int i = 0; i < N_BUF; i++) {
        s_buf[i] = heap_caps_malloc(GFX_W * STRIP_H * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        assert(s_buf[i] != NULL);
    }
    backlight_init();
    backlight_apply();
}

void display_draw(const gfx_t *g)
{
    if (!s_on) {
        return;
    }
    for (int i = 0, y = 0; y < GFX_H; i++, y += STRIP_H) {
        uint16_t *buf = s_buf[i % N_BUF];
        xSemaphoreTake(s_free, portMAX_DELAY);  // Puffer erst nach Ende der vorherigen DMA-Übertragung
        gfx_render(g, y, STRIP_H, buf, true);
        esp_lcd_panel_draw_bitmap(s_panel, 0, y, GFX_W, y + STRIP_H, buf);
    }
}

void display_backlight(uint8_t percent)
{
    s_brightness = percent > 100 ? 100 : percent;
    backlight_apply();
}

void display_power(bool on)
{
    s_on = on;
    esp_lcd_panel_disp_on_off(s_panel, on);
    backlight_apply();
}
