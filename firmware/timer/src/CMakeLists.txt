file(GLOB FONT_SRCS "${CMAKE_CURRENT_LIST_DIR}/ui/fonts/*.c")
idf_component_register(SRCS "main.c"
                            "core/settings.c" "core/shot_string.c" "core/timer_core.c"
                            "ui/ui.c" "ui/gfx.c" ${FONT_SRCS}
                            "hal/buttons.c" "hal/display.c"
                            "svc/config_store.c"
                       INCLUDE_DIRS "." "core" "ui" "hal" "svc"
                       REQUIRES board envelope beeper driver esp_timer esp_lcd nvs_flash)
