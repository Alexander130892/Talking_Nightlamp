idf_component_register(
    SRCS "lamp_strip.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio logic
    PRIV_REQUIRES esp_driver_rmt
)
