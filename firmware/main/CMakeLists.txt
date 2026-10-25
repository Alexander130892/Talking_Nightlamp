idf_component_register(
    SRCS "main.c" "app.c"
    INCLUDE_DIRS "."
    REQUIRES logic rc522 storage audio lamp_strip input
)
