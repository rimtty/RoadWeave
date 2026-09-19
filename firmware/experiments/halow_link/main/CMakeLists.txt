idf_component_register(
    SRCS "app_main.c" "preflight.c" "radio_test.c" "validation_run.c" "throughput_run.c" "dhcp_netif.c" "status_led.c" "diagnostic_log.c" "board_compat.c" "vendor/sdio_spi.c"
    INCLUDE_DIRS "." "vendor"
    REQUIRES halow esp_netif esp_event nvs_flash esp_timer esp_hw_support esp_psram
)
# The SDK links its processed archive after IDF's component group.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--undefined=mm_hexdump")
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=mmhal_wlan_busy_is_asserted" "-Wl,--undefined=__wrap_mmhal_wlan_busy_is_asserted")
if(CONFIG_RW_LINK_DHCP AND CONFIG_RW_LINK_AP)
    target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_netif_new" "-Wl,--undefined=__wrap_esp_netif_new")
endif()
