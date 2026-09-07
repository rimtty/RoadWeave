#include <stdatomic.h>
#include <stdio.h>
#include "driver/gpio.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "link_test.h"

/* XIAO ESP32S3 user LED: GPIO21, active low. */
static atomic_int mode = RW_LED_WAIT;
static atomic_uint activity_tick;

static void led_task(void *arg)
{
    (void)arg;
    for (;;) {
        unsigned now = xTaskGetTickCount();
        unsigned ms = now * portTICK_PERIOD_MS;
        enum rw_led_mode current = atomic_load(&mode);
        bool on = false;
        if (current == RW_LED_WAIT) on = ms % 1000 < 500;
        if (current == RW_LED_CONNECTED)
            on = now - atomic_load(&activity_tick) >= pdMS_TO_TICKS(80);
        if (current == RW_LED_PASS) on = ms % 2000 < 150 ||
                                         (ms % 2000 >= 300 && ms % 2000 < 450);
        if (current == RW_LED_FAIL) on = ms % 1600 < 600 && ms % 200 < 100;
        gpio_set_level(GPIO_NUM_21, !on);
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

void status_led_init(void)
{
    gpio_config_t config = {.pin_bit_mask = 1ULL << GPIO_NUM_21, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&config));
    gpio_set_level(GPIO_NUM_21, 1);
    configASSERT(xTaskCreate(led_task, "link_led", 2048, NULL, 1, NULL) == pdPASS);
    printf("RW_LINK_LED gpio=21 active_low=1 wait=slow connected=on traffic=short_off pass=double fail=triple\n");
}

void status_led_set(enum rw_led_mode value) { atomic_store(&mode, value); }
void status_led_activity(void) { atomic_store(&activity_tick, xTaskGetTickCount()); }
