idf_component_register(
    SRCS 
        "opentherm.c"
    
    INCLUDE_DIRS 
        "include"

    REQUIRES 
        driver
        esp_timer
)