idf_component_register(SRCS "voice_udp.c" INCLUDE_DIRS "."
    REQUIRES rwp audio_pipeline esp_wifi nvs_flash esp_netif esp_event lwip esp_driver_i2s esp_driver_gpio esp_timer)
