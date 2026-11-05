idf_component_register(
    SRCS "lamp.c"
    INCLUDE_DIRS "include"
    REQUIRES lamp_strip
    PRIV_REQUIRES logic esp_timer
)
