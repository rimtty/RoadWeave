// RoadWeave UI experiment: ILI9341 2.8in SPI + XPT2046 touch via esp_lcd + esp_lvgl_port (LVGL 9).
// Runs on core 1; with RW_UI_SIMULATE it animates a 3-car convoy so the screens can be tuned before GPS/radio exist.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_touch_xpt2046.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "ui_screens.h"
#include "ui_simulation.h"
#include "position.h"
#include "sdkconfig.h"

static const char *TAG = "ui";
#define LCD_H_RES 240
#define LCD_V_RES 320
#define LCD_HOST  SPI2_HOST

static void backlight_init(void)
{
#if CONFIG_RW_LCD_BL_GPIO >= 0
    ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT, .timer_num = LEDC_TIMER_0,
                              .freq_hz = 25000, .clk_cfg = LEDC_AUTO_CLK };   // >20 kHz: keep PWM out of the audio band
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t c = { .gpio_num = CONFIG_RW_LCD_BL_GPIO, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_0,
                                .timer_sel = LEDC_TIMER_0, .duty = 0, .hpoint = 0 };
    ESP_ERROR_CHECK(ledc_channel_config(&c));
#endif
}
static void backlight_set(int percent)
{
#if CONFIG_RW_LCD_BL_GPIO >= 0
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (uint32_t)(1023 * percent / 100));
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#else
    (void)percent;
#endif
}

static lv_display_t *display_init(void)
{
    spi_bus_config_t bus = { .sclk_io_num = CONFIG_RW_LCD_SCLK_GPIO, .mosi_io_num = CONFIG_RW_LCD_MOSI_GPIO,
                             .miso_io_num = CONFIG_RW_LCD_MISO_GPIO, .quadwp_io_num = -1, .quadhd_io_num = -1,
                             .max_transfer_sz = LCD_H_RES * 40 * 2 };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io; esp_lcd_panel_handle_t panel;
    esp_lcd_panel_io_spi_config_t io_cfg = { .dc_gpio_num = CONFIG_RW_LCD_DC_GPIO, .cs_gpio_num = CONFIG_RW_LCD_CS_GPIO,
                                             .pclk_hz = CONFIG_RW_LCD_SPI_MHZ * 1000 * 1000, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
                                             .spi_mode = 0, .trans_queue_depth = 10 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));
    esp_lcd_panel_dev_config_t pcfg = { .reset_gpio_num = CONFIG_RW_LCD_RST_GPIO, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR, .bits_per_pixel = 16 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io, &pcfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));

    const lvgl_port_cfg_t lv_cfg = { .task_priority = 4, .task_stack = 8192, .task_affinity = 1, .task_max_sleep_ms = 500, .timer_period_ms = 5 };
    ESP_ERROR_CHECK(lvgl_port_init(&lv_cfg));
    const lvgl_port_display_cfg_t dcfg = {
        .io_handle = io, .panel_handle = panel, .buffer_size = LCD_H_RES * 40, .double_buffer = true,
        .hres = LCD_H_RES, .vres = LCD_V_RES, .monochrome = false,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = { .buff_dma = true, .swap_bytes = true },
    };
    lv_display_t *disp = lvgl_port_add_disp(&dcfg);

#if CONFIG_RW_TOUCH_CS_GPIO >= 0
    esp_lcd_panel_io_handle_t tio;
    esp_lcd_panel_io_spi_config_t tio_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(CONFIG_RW_TOUCH_CS_GPIO);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &tio_cfg, &tio));
    esp_lcd_touch_config_t tcfg = { .x_max = LCD_H_RES, .y_max = LCD_V_RES, .rst_gpio_num = -1, .int_gpio_num = CONFIG_RW_TOUCH_IRQ_GPIO,
                                    .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 } };
    esp_lcd_touch_handle_t touch;
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tio, &tcfg, &touch));
    const lvgl_port_touch_cfg_t lt = { .disp = disp, .handle = touch };
    lvgl_port_add_touch(&lt);
#endif
    return disp;
}

static void on_action(const char *a, uint32_t arg) { ESP_LOGI(TAG, "action %s %lu", a, (unsigned long)arg); }


void app_main(void)
{
    backlight_init();
    lv_display_t *disp = display_init();
    backlight_set(80);
    ui_set_action_cb(on_action);
    if (lvgl_port_lock(0)) { ui_create(disp); lvgl_port_unlock(); }
    ESP_LOGI(TAG, "UI up: %dx%d, SPI %d MHz, simulate=%d", LCD_H_RES, LCD_V_RES, CONFIG_RW_LCD_SPI_MHZ, CONFIG_RW_UI_SIMULATE);

    static ui_model_t model; int64_t t0 = esp_timer_get_time(); uint32_t frames = 0;
    for (;;) {
        float t = (float)((esp_timer_get_time() - t0) / 1000000.0);
#if CONFIG_RW_UI_SIMULATE
        ui_simulate(&model, t);
#endif
        if (lvgl_port_lock(50)) { ui_update(&model); lvgl_port_unlock(); }
        if (++frames % 50 == 0) ESP_LOGI(TAG, "screen %d, %lu updates, %.1f s", ui_current(), (unsigned long)frames, (double)t);
        vTaskDelay(pdMS_TO_TICKS(100));   // 10 Hz model updates; LVGL renders on its own task
    }
}
