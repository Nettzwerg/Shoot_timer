#include "config_store.h"

#include "nvs.h"

#define NS  "timer"
#define KEY "settings"

void config_load(settings_t *s)
{
    nvs_handle_t h;
    size_t len = sizeof(*s);
    bool ok = false;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        ok = nvs_get_blob(h, KEY, s, &len) == ESP_OK && len == sizeof(*s);
        nvs_close(h);
    }
    if (!ok) {
        settings_defaults(s);
    }
    settings_sanitize(s);
}

bool config_save(const settings_t *s)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool ok = nvs_set_blob(h, KEY, s, sizeof(*s)) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}
