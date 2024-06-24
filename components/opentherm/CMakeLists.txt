idf_component_register(
    SRCS opentherm.c
    INCLUDE_DIRS .
    PRIV_REQUIRES driver esp_timer log
)