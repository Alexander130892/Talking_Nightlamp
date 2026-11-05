idf_component_register(
    SRCS "main.c" "app.c"
    INCLUDE_DIRS "."
    REQUIRES logic rfid storage audio lamp input esp_timer
)
