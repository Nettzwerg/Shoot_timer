#include "config_store.h"

#include <stdio.h>

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

static bool blob_save(const char *key, const settings_t *s)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        return false;
    }
    bool ok = nvs_set_blob(h, key, s, sizeof(*s)) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool config_save(const settings_t *s)
{
    return blob_save(KEY, s);
}

bool preset_load(uint8_t slot, settings_t *s)
{
    char key[12];
    snprintf(key, sizeof(key), "preset%u", slot);
    nvs_handle_t h;
    size_t len = sizeof(*s);
    bool ok = false;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        ok = nvs_get_blob(h, key, s, &len) == ESP_OK && len == sizeof(*s) && s->version == SETTINGS_VERSION;
        nvs_close(h);
    }
    if (ok) {
        settings_sanitize(s);
    }
    return ok;
}

bool preset_store(uint8_t slot, const settings_t *s)
{
    char key[12];
    snprintf(key, sizeof(key), "preset%u", slot);
    return blob_save(key, s);
}
