#include <stdio.h>
#include "esp_mac.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "link_test.h"

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(1200));
    status_led_init();
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
#else
    printf("RW_LINK_RF=DISABLED; no radio firmware boot, scan, association or TX\n");
    bool pass = run_preflight();
    printf("RW_LINK_PREFLIGHT=%s\n", pass ? "PASS" : "FAIL");
    printf("RW_LINK_RADIO_RESULT=NOT_RUN\n");
#endif
    status_led_set(pass ? RW_LED_PASS : RW_LED_FAIL);
    printf("RW_LINK_DONE\n");
}
