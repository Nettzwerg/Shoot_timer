#include "timer_core.h"

#include <string.h>

void tc_init(timer_core_t *tc, uint32_t (*rand32)(void), uint32_t next_seq)
{
    memset(tc, 0, sizeof(*tc));
    tc->rand32 = rand32;
    tc->seq = next_seq;
    settings_defaults(&tc->cfg);
}

static uint32_t pick_delay_ms(timer_core_t *tc)
{
    const settings_t *c = &tc->cfg;
    switch (c->delay_type) {
    case DELAY_FIXED:
        return c->delay_fixed_ms;
    case DELAY_INSTANT:
        return 0;
    case DELAY_RANDOM:
    default: {
        uint32_t span = c->delay_max_ms > c->delay_min_ms ? c->delay_max_ms - c->delay_min_ms : 0;
        return c->delay_min_ms + (span ? tc->rand32() % (span + 1) : 0);
    }
    }
}

static uint32_t enter_delay(timer_core_t *tc, int64_t now_us)
{
    uint32_t ms = pick_delay_ms(tc);
    tc->state = TC_DELAY;
    tc->delay_start_us = now_us;
    tc->delay_end_us = now_us + (int64_t)ms * 1000;
    return TC_ACT_CHANGED;
}

static uint32_t enter_run(timer_core_t *tc, int64_t now_us, bool beep)
{
    shot_string_t *r = &tc->run;
    ss_clear(r);
    r->mode = tc->cfg.mode;
    r->par_count = settings_active_pars(&tc->cfg, r->par_ms);
    r->delay_ms = (uint32_t)((now_us - tc->delay_start_us) / 1000);
    r->power_factor = tc->cfg.power_factor;
    tc->state = TC_RUN;
    tc->t0_us = now_us;
    tc->par_idx = 0;
    tc->warned = false;
    uint32_t act = TC_ACT_CHANGED;
    if (beep) {
        act |= TC_ACT_BEEP_START;
    }
    if (tc->cfg.mode != MODE_DRY) {
        act |= TC_ACT_DETECT_ON;
    }
    return act;
}

static uint32_t finish_run(timer_core_t *tc, int64_t now_us, tc_end_reason_t reason)
{
    tc->run.seq = tc->seq++;
    tc->last = tc->run;
    tc->has_last = true;
    tc->end_reason = reason;
    uint32_t act = TC_ACT_STRING_DONE | TC_ACT_DETECT_OFF | TC_ACT_CHANGED;

    bool again = false;
    if (reason != TC_END_KEY) {
        if (tc->cfg.mode == MODE_REPEAT) {
            again = true;
        } else if (tc->cfg.mode == MODE_DRY && tc->reps_left > 1) {
            tc->reps_left--;
            again = true;
        }
    }
    if (again) {
        tc->state = TC_PAUSE;
        tc->pause_end_us = now_us + (int64_t)tc->cfg.repeat_pause_s * 1000000;
    } else {
        tc->state = TC_READY;
    }
    return act;
}

uint32_t tc_start_key(timer_core_t *tc, const settings_t *settings, int64_t now_us)
{
    switch (tc->state) {
    case TC_READY:
        tc->cfg = *settings;
        tc->reps_left = tc->cfg.mode == MODE_DRY ? tc->cfg.dry_reps : 0;
        if (tc->cfg.mode == MODE_SPY) {
            tc->delay_start_us = now_us;
            return enter_run(tc, now_us, false);
        }
        return enter_delay(tc, now_us);
    case TC_DELAY:
    case TC_PAUSE:
        tc->state = TC_READY;   // Abbruch, kein String
        return TC_ACT_CHANGED;
    case TC_RUN:
        return finish_run(tc, now_us, TC_END_KEY);
    }
    return 0;
}

uint32_t tc_shot(timer_core_t *tc, int64_t shot_us, int8_t db)
{
    if (tc->state != TC_RUN || tc->cfg.mode == MODE_DRY || shot_us < tc->t0_us) {
        return 0;
    }
    if (!ss_add(&tc->run, (uint32_t)(shot_us - tc->t0_us), db)) {
        return finish_run(tc, shot_us, TC_END_FULL);
    }
    return TC_ACT_SHOT_ADDED | TC_ACT_CHANGED;
}

static int64_t last_par_us(const timer_core_t *tc)
{
    const shot_string_t *r = &tc->run;
    return r->par_count ? (int64_t)r->par_ms[r->par_count - 1] * 1000 : 0;
}

// Bezugspunkt für den Auto-Stopp: letzter Schuss bzw. letzte Par-Zeit, relativ zu t0
static int64_t auto_stop_ref_us(const timer_core_t *tc)
{
    int64_t shot = tc->run.n ? (int64_t)ss_total_us(&tc->run) : 0;
    int64_t par = last_par_us(tc);
    return shot > par ? shot : par;
}

uint32_t tc_tick(timer_core_t *tc, int64_t now_us)
{
    uint32_t act = 0;
    switch (tc->state) {
    case TC_READY:
        break;

    case TC_DELAY:
        if (now_us >= tc->delay_end_us) {
            act |= enter_run(tc, now_us, true);
        }
        break;

    case TC_PAUSE:
        if (now_us >= tc->pause_end_us) {
            act |= enter_delay(tc, now_us);
        }
        break;

    case TC_RUN: {
        int64_t t = now_us - tc->t0_us;
        const shot_string_t *r = &tc->run;
        while (tc->par_idx < r->par_count && t >= (int64_t)r->par_ms[tc->par_idx] * 1000) {
            tc->par_idx++;
            act |= TC_ACT_BEEP_PAR | TC_ACT_CHANGED;
        }
        const settings_t *c = &tc->cfg;
        if (c->mode == MODE_COUNTDOWN) {
            int64_t end = (int64_t)c->countdown_s * 1000000;
            if (c->countdown_warn_s && c->countdown_warn_s < c->countdown_s && !tc->warned &&
                t >= end - (int64_t)c->countdown_warn_s * 1000000) {
                tc->warned = true;
                act |= TC_ACT_BEEP_WARN;
            }
            if (t >= end) {
                return act | TC_ACT_BEEP_END | finish_run(tc, now_us, TC_END_COUNTDOWN);
            }
        } else if (c->mode == MODE_DRY) {
            int64_t end = r->par_count ? last_par_us(tc) + TC_DRY_END_US : TC_DRY_NOPAR_US;
            if (t >= end) {
                return act | finish_run(tc, now_us, TC_END_DRY);
            }
        } else if (c->auto_stop_s && t - auto_stop_ref_us(tc) >= (int64_t)c->auto_stop_s * 1000000) {
            return act | finish_run(tc, now_us, TC_END_AUTO_STOP);
        }
        if (t >= TC_MAX_TIME_US) {
            return act | finish_run(tc, now_us, TC_END_MAX_TIME);
        }
        break;
    }
    }
    return act;
}

int64_t tc_next_deadline(const timer_core_t *tc)
{
    switch (tc->state) {
    case TC_DELAY:
        return tc->delay_end_us;
    case TC_PAUSE:
        return tc->pause_end_us;
    case TC_RUN: {
        const settings_t *c = &tc->cfg;
        const shot_string_t *r = &tc->run;
        int64_t next = tc->t0_us + TC_MAX_TIME_US;
        if (tc->par_idx < r->par_count) {
            int64_t p = tc->t0_us + (int64_t)r->par_ms[tc->par_idx] * 1000;
            next = p < next ? p : next;
        }
        int64_t end;
        if (c->mode == MODE_COUNTDOWN) {
            end = tc->t0_us + (int64_t)c->countdown_s * 1000000;
            if (c->countdown_warn_s && c->countdown_warn_s < c->countdown_s && !tc->warned) {
                end -= (int64_t)c->countdown_warn_s * 1000000;
            }
        } else if (c->mode == MODE_DRY) {
            end = tc->t0_us + (r->par_count ? last_par_us(tc) + TC_DRY_END_US : TC_DRY_NOPAR_US);
        } else if (c->auto_stop_s) {
            end = tc->t0_us + auto_stop_ref_us(tc) + (int64_t)c->auto_stop_s * 1000000;
        } else {
            end = next;
        }
        return end < next ? end : next;
    }
    case TC_READY:
    default:
        return INT64_MAX;
    }
}

void tc_set_t0(timer_core_t *tc, int64_t t0_us)
{
    if (tc->state == TC_RUN) {
        tc->t0_us = t0_us;
    }
}

uint32_t tc_elapsed_us(const timer_core_t *tc, int64_t now_us)
{
    return tc->state == TC_RUN && now_us > tc->t0_us ? (uint32_t)(now_us - tc->t0_us) : 0;
}
