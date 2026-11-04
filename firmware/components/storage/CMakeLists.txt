idf_component_register(
    SRCS "storage.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi
    PRIV_REQUIRES fatfs sdmmc esp_driver_sdspi
)
