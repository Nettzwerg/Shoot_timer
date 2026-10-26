#include "settings.h"

#include <string.h>

void settings_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->version = SETTINGS_VERSION;
    s->mode = MODE_STANDARD;
    s->delay_type = DELAY_RANDOM;
    s->delay_min_ms = 1000;
    s->delay_max_ms = 4000;
    s->delay_fixed_ms = 3000;
    s->par_count = 1;
    for (int i = 0; i < PAR_MAX; i++) {
        s->par_ms[i] = 2000 * (i + 1);
    }
    s->par_in_repeat = true;
    s->sensitivity = 6;
    s->echo_ms = 50;
    s->auto_stop_s = 5;
    s->beep_hz = 3000;
    s->beep_ms = 300;
    s->beep_volume = 2;
    s->repeat_pause_s = 5;
    s->countdown_s = 30;
    s->countdown_warn_s = 10;
    s->dry_reps = 5;
    s->power_factor = PF_MINOR;
    s->brightness = 100;
    s->auto_off_min = 15;
    s->wifi = false;
}

static bool clamp_u8(uint8_t *v, uint8_t lo, uint8_t hi)
{
    uint8_t old = *v;
    *v = *v < lo ? lo : *v > hi ? hi : *v;
    return *v != old;
}

static bool clamp_u16(uint16_t *v, uint16_t lo, uint16_t hi)
{
    uint16_t old = *v;
    *v = *v < lo ? lo : *v > hi ? hi : *v;
    return *v != old;
}

static bool clamp_u32(uint32_t *v, uint32_t lo, uint32_t hi)
{
    uint32_t old = *v;
    *v = *v < lo ? lo : *v > hi ? hi : *v;
    return *v != old;
}

bool settings_sanitize(settings_t *s)
{
    bool changed = false;
    if (s->version != SETTINGS_VERSION) {
        settings_defaults(s);
        return true;
    }
    changed |= clamp_u8(&s->mode, 0, MODE_COUNT - 1);
    changed |= clamp_u8(&s->delay_type, 0, DELAY_COUNT - 1);
    changed |= clamp_u16(&s->delay_min_ms, 500, 10000);
    changed |= clamp_u16(&s->delay_max_ms, 500, 10000);
    if (s->delay_max_ms < s->delay_min_ms) {
        s->delay_max_ms = s->delay_min_ms;
        changed = true;
    }
    changed |= clamp_u16(&s->delay_fixed_ms, 0, 10000);
    changed |= clamp_u8(&s->par_count, 1, PAR_MAX);
    for (int i = 0; i < PAR_MAX; i++) {
        changed |= clamp_u32(&s->par_ms[i], 100, 99990);
    }
    changed |= clamp_u8(&s->sensitivity, 1, 10);
    changed |= clamp_u16(&s->echo_ms, 20, 500);
    changed |= clamp_u8(&s->auto_stop_s, 0, 60);
    changed |= clamp_u16(&s->beep_hz, 1000, 4000);
    changed |= clamp_u16(&s->beep_ms, 100, 1000);
    changed |= clamp_u8(&s->beep_volume, 0, 2);
    changed |= clamp_u8(&s->repeat_pause_s, 3, 60);
    changed |= clamp_u16(&s->countdown_s, 5, 300);
    changed |= clamp_u8(&s->countdown_warn_s, 0, 30);
    changed |= clamp_u8(&s->dry_reps, 1, 20);
    changed |= clamp_u8(&s->power_factor, PF_MINOR, PF_MAJOR);
    changed |= clamp_u8(&s->brightness, 20, 100);
    changed |= clamp_u8(&s->auto_off_min, 5, 60);
    return changed;
}

float settings_threshold_dbfs(uint8_t sensitivity)
{
    if (sensitivity < 1) {
        sensitivity = 1;
    } else if (sensitivity > 10) {
        sensitivity = 10;
    }
    return -6.0f - (sensitivity - 1) * 34.0f / 9.0f;
}

uint8_t settings_active_pars(const settings_t *s, uint32_t out_ms[PAR_MAX])
{
    bool active = s->mode == MODE_PAR || s->mode == MODE_DRY ||
                  (s->mode == MODE_REPEAT && s->par_in_repeat);
    if (!active) {
        return 0;
    }
    uint8_t n = s->par_count > PAR_MAX ? PAR_MAX : s->par_count;
    for (uint8_t i = 0; i < n; i++) {
        out_ms[i] = s->par_ms[i];
    }
    // Einfügesortierung, n <= 10
    for (uint8_t i = 1; i < n; i++) {
        uint32_t v = out_ms[i];
        int j = i - 1;
        while (j >= 0 && out_ms[j] > v) {
            out_ms[j + 1] = out_ms[j];
            j--;
        }
        out_ms[j + 1] = v;
    }
    return n;
}
