# Hardware-independent logic. Must not include any ESP-IDF header:
# the same sources are compiled on a PC by test/host/Makefile.
idf_component_register(
    SRCS "tag_format.c" "card_fsm.c" "lamp_model.c" "power_limit.c"
    INCLUDE_DIRS "include"
)
