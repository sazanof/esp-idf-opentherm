idf_component_register(
    SRCS 
        "src/opentherm.c"
    
    INCLUDE_DIRS 
        "include"

    REQUIRES 
        driver
        esp_timer
)