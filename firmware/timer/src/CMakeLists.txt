idf_component_register(SRCS "main.c"
                            "core/settings.c" "core/shot_string.c" "core/timer_core.c"
                            "hal/buttons.c"
                            "svc/config_store.c"
                       INCLUDE_DIRS "." "core" "hal" "svc"
                       REQUIRES board envelope beeper driver esp_timer nvs_flash)
