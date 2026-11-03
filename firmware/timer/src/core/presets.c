#include "presets.h"

#include <stdio.h>
#include <string.h>

void preset_apply(settings_t *dst, const settings_t *p)
{
    dst->mode = p->mode;
    dst->delay_type = p->delay_type;
    dst->delay_min_ms = p->delay_min_ms;
    dst->delay_max_ms = p->delay_max_ms;
    dst->delay_fixed_ms = p->delay_fixed_ms;
    dst->par_count = p->par_count;
    memcpy(dst->par_ms, p->par_ms, sizeof(dst->par_ms));
    dst->par_in_repeat = p->par_in_repeat;
    dst->repeat_pause_s = p->repeat_pause_s;
    dst->countdown_s = p->countdown_s;
    dst->countdown_warn_s = p->countdown_warn_s;
    dst->dry_reps = p->dry_reps;
    settings_sanitize(dst);
}

void preset_name(const settings_t *p, char *buf, size_t len)
{
    static const char *const names[MODE_COUNT] = {
        "Standard", "Par", "Spy", "Trocken", "Wiederh.", "Countdown",
    };
    const char *mode = p->mode < MODE_COUNT ? names[p->mode] : "?";
    uint32_t pars[PAR_MAX];
    uint8_t n = settings_active_pars(p, pars);
    if (p->mode == MODE_COUNTDOWN) {
        snprintf(buf, len, "%s %u s", mode, p->countdown_s);
    } else if (n == 1) {
        snprintf(buf, len, "%s %.2f s", mode, pars[0] / 1000.0);
    } else if (n > 1) {
        snprintf(buf, len, "%s %u Par-Zeiten", mode, n);
    } else {
        snprintf(buf, len, "%s", mode);
    }
}
