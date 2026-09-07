idf_component_register(
    SRCS "app_main.c" "preflight.c" "radio_test.c" "status_led.c" "diagnostic_log.c" "vendor/sdio_spi.c"
    INCLUDE_DIRS "." "vendor"
    REQUIRES halow esp_netif esp_event nvs_flash esp_timer esp_hw_support esp_psram
)
# The SDK links its processed archive after IDF's component group.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--undefined=mm_hexdump")
