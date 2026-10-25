idf_component_register(SRCS "main.c" "envelope.c" "beeper.c" "storage.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_adc esp_timer spiffs nvs_flash)
