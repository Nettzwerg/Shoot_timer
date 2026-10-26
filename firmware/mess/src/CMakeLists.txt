idf_component_register(SRCS "main.c" "storage.c" "web.c"
                       INCLUDE_DIRS "."
                       REQUIRES board envelope beeper driver esp_adc esp_timer spiffs nvs_flash
                                esp_wifi esp_netif esp_event esp_http_server
                       EMBED_TXTFILES "index.html")
