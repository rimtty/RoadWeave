#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"

static const char *TAG = "roadweave";

static void log_storage(void)
{
    uint32_t flash_size = 0;
    esp_err_t flash_result = esp_flash_get_size(NULL, &flash_size);

    if (flash_result == ESP_OK) {
        ESP_LOGI(TAG, "Flash: %" PRIu32 " bytes (%" PRIu32 " MiB)", flash_size,
                 flash_size / (1024U * 1024U));
    } else {
        ESP_LOGE(TAG, "Flash size read failed: %s", esp_err_to_name(flash_result));
    }

    const bool psram_ready = esp_psram_is_initialized();
    const size_t psram_physical = psram_ready ? esp_psram_get_size() : 0;
    const size_t psram_heap_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG,
             "PSRAM: initialized=%s physical=%u bytes heap_total=%u bytes free=%u bytes",
             psram_ready ? "yes" : "no", (unsigned int)psram_physical,
             (unsigned int)psram_heap_total, (unsigned int)psram_free);

    if (flash_result == ESP_OK && flash_size >= (8U * 1024U * 1024U) && psram_ready &&
        psram_physical >= (8U * 1024U * 1024U)) {
        ESP_LOGI(TAG, "P0A_XIAO_SMOKE=PASS");
    } else {
        ESP_LOGE(TAG, "P0A_XIAO_SMOKE=FAIL expected at least 8 MiB flash and 8 MiB PSRAM");
    }
}

void app_main(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "RoadWeave P0-A XIAO board smoke test");
    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Chip: model=%d cores=%d revision=%d", chip_info.model,
             chip_info.cores, chip_info.revision);
    ESP_LOGI(TAG, "Internal heap: free=%" PRIu32 " minimum=%" PRIu32 " bytes",
             esp_get_free_heap_size(), esp_get_minimum_free_heap_size());

    log_storage();

    ESP_LOGW(TAG, "HALOW_TX=DISABLED: this firmware does not initialize WM6180/MM6108");
    ESP_LOGI(TAG, "Record this complete boot log in docs/bringup/p0-a-test-log.md");
}
