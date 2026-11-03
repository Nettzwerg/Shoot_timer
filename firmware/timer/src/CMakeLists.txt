file(GLOB FONT_SRCS "${CMAKE_CURRENT_LIST_DIR}/ui/fonts/*.c")
idf_component_register(SRCS "main.c"
                            "core/settings.c" "core/shot_string.c" "core/timer_core.c"
                            "core/history.c" "core/presets.c" "core/power.c"
                            "ui/ui.c" "ui/gfx.c" ${FONT_SRCS}
                            "hal/buttons.c" "hal/display.c" "hal/sleep.c"
                            "svc/config_store.c" "svc/history_flash.c"
                       INCLUDE_DIRS "." "core" "ui" "hal" "svc"
                       REQUIRES board envelope beeper driver esp_timer esp_lcd esp_partition nvs_flash)
