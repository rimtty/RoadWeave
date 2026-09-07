idf_component_register(SRCS "ui_main.c" "ui_screens.c" "ui_simulation.c" "ui_dot.c"
    "fonts/rw_dot_font_7.c" "fonts/rw_dot_font_8.c" "fonts/rw_dot_font_9.c"
    "fonts/rw_dot_font_10.c" "fonts/rw_dot_font_18.c" INCLUDE_DIRS "."
    REQUIRES position esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_timer)
