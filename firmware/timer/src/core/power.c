#include "power.h"

#include <string.h>

#define FILTER  0.2f    // gleitender Mittelwert der Spannung (bei 1 Messung/s ca. 5 s)

void pwr_init(power_t *p, int64_t now_us)
{
    memset(p, 0, sizeof(*p));
    p->last_activity_us = now_us;
}

uint32_t pwr_activity(power_t *p, int64_t now_us)
{
    p->last_activity_us = now_us;
    if (p->dimmed) {
        p->dimmed = false;
        return PWR_ACT_UNDIM;
    }
    return 0;
}

uint32_t pwr_tick(power_t *p, int64_t now_us, bool busy, uint8_t auto_off_min)
{
    if (busy) {
        p->last_activity_us = now_us;
        return 0;
    }
    int64_t idle = now_us - p->last_activity_us;
    uint32_t act = 0;
    if (!p->dimmed && idle >= PWR_DIM_US) {
        p->dimmed = true;
        act |= PWR_ACT_DIM;
    }
    if (auto_off_min && idle >= (int64_t)auto_off_min * 60000000LL) {
        act |= PWR_ACT_OFF;
    }
    return act;
}

uint32_t pwr_battery(power_t *p, float v, int64_t now_us)
{
    // Ohne Akku steht der Eingang auf 0 V; das Filter würde sonst minutenlang "leer" melden
    if (v < PWR_NO_BATT_V) {
        p->v = v;
        p->v_valid = true;
        p->state = BATT_NONE;
        p->crit_since_us = 0;
        return 0;
    }
    if (!p->v_valid || p->state == BATT_NONE) {
        p->v = v;
        p->v_valid = true;
    } else {
        p->v += (v - p->v) * FILTER;
    }

    uint32_t act = 0;
    if (p->v < PWR_CRIT_V) {
        if (p->crit_since_us == 0) {
            p->crit_since_us = now_us;
        }
        if (now_us - p->crit_since_us >= PWR_CRIT_HOLD_US) {
            p->state = BATT_CRITICAL;
            if (!p->shutdown_sent) {
                p->shutdown_sent = true;
                act |= PWR_ACT_SHUTDOWN;
            }
            return act;
        }
    } else {
        p->crit_since_us = 0;
        p->shutdown_sent = false;
    }

    if (p->v < PWR_LOW_V) {
        p->state = BATT_LOW;
        if (!p->warned) {
            p->warned = true;
            act |= PWR_ACT_LOW_WARN;
        }
    } else if (p->v >= PWR_LOW_CLEAR_V || p->state == BATT_NONE) {
        p->state = BATT_OK;
        p->warned = false;
    } else if (p->state != BATT_LOW) {
        p->state = BATT_OK;     // zwischen den Schwellen: Zustand halten, Start mit OK
    }
    return act;
}

uint8_t pwr_percent(float v)
{
    static const struct {
        float v;
        uint8_t pct;
    } curve[] = {
        { 3.30f, 0 }, { 3.40f, 3 }, { 3.50f, 6 }, { 3.60f, 12 }, { 3.65f, 20 }, { 3.70f, 30 },
        { 3.75f, 40 }, { 3.80f, 50 }, { 3.90f, 65 }, { 4.00f, 80 }, { 4.10f, 90 }, { 4.20f, 100 },
    };
    const int n = sizeof(curve) / sizeof(curve[0]);
    if (v <= curve[0].v) {
        return 0;
    }
    if (v >= curve[n - 1].v) {
        return 100;
    }
    for (int i = 1; i < n; i++) {
        if (v < curve[i].v) {
            float f = (v - curve[i - 1].v) / (curve[i].v - curve[i - 1].v);
            return (uint8_t)(curve[i - 1].pct + f * (curve[i].pct - curve[i - 1].pct) + 0.5f);
        }
    }
    return 100;
}
