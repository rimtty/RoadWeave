idf_component_register(
    SRCS "roadweave_main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_psram spi_flash
)
