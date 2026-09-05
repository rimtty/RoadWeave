idf_component_register(SRCS "ui_main.c" "ui_screens.c" INCLUDE_DIRS "."
    REQUIRES position esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_timer)
