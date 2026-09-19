#include <stdio.h>
#include "esp_mac.h"
#include "esp_err.h"
#include "esp_psram.h"
#include "esp_random.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "link_test.h"

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1200));
    /* CPU-only reset can retain the MM6108 active-low IRQ GPIO setting.
     * Clear it before either RF or preflight installs the SDK ISR service. */
    ESP_ERROR_CHECK(gpio_intr_disable(CONFIG_MM_SPI_IRQ));
    printf("RW_LINK_IRQ_QUIESCED gpio=%d\n", CONFIG_MM_SPI_IRQ);
    status_led_init();
#ifdef CONFIG_RW_LINK_CONTINUOUS
    printf("RW_LINK_BOOT boot_id=%08x reset_reason=%d\n", (unsigned)esp_random(), (int)esp_reset_reason());
#endif
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    printf("RW_LINK_ROLE=%s ESP32_MAC=" MACSTR " PSRAM_BYTES=%u\n",
#ifdef CONFIG_RW_LINK_AP
           "AP",
#else
           "STA",
#endif
           MAC2STR(mac), (unsigned)esp_psram_get_size());
#ifdef CONFIG_RW_LINK_RF_ENABLE
    printf("RW_LINK_RF=ENABLED\n");
    bool pass = run_radio_test();
    printf("RW_LINK_RADIO_RESULT=%s\n", pass ? "PASS" : "FAIL");
#ifdef CONFIG_RW_LINK_CONTINUOUS
    if (validation_restart_requested()) {
        if (validation_radio_shutdown_ok()) {
            printf("RW_LINK_RESTART_READY radio_stopped=1\n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
        }
        printf("RW_LINK_RESTART_ABORT radio_stopped=0\n");
    }
#endif
#else
    printf("RW_LINK_RF=DISABLED; no radio firmware boot, scan, association or TX\n");
    bool pass = run_preflight();
    printf("RW_LINK_PREFLIGHT=%s\n", pass ? "PASS" : "FAIL");
    printf("RW_LINK_RADIO_RESULT=NOT_RUN\n");
#endif
    status_led_set(pass ? RW_LED_PASS : RW_LED_FAIL);
    printf("RW_LINK_DONE\n");
}
