#include "shot_string.h"

#include <stdio.h>
#include <string.h>

void ss_clear(shot_string_t *s)
{
    memset(s, 0, sizeof(*s));
}

bool ss_add(shot_string_t *s, uint32_t t_us, int8_t db)
{
    if (s->n >= SHOTS_MAX) {
        return false;
    }
    s->shots[s->n].t_us = t_us;
    s->shots[s->n].db = db;
    s->n++;
    return true;
}

bool ss_delete_last(shot_string_t *s)
{
    if (s->n == 0) {
        return false;
    }
    s->n--;
    return true;
}

uint32_t ss_first_us(const shot_string_t *s)
{
    return s->n ? s->shots[0].t_us : 0;
}

uint32_t ss_total_us(const shot_string_t *s)
{
    return s->n ? s->shots[s->n - 1].t_us : 0;
}

uint32_t ss_split_us(const shot_string_t *s, uint8_t i)
{
    if (i >= s->n) {
        return 0;
    }
    return i == 0 ? s->shots[0].t_us : s->shots[i].t_us - s->shots[i - 1].t_us;
}

uint32_t ss_fastest_split_us(const shot_string_t *s)
{
    uint32_t best = 0;
    for (uint8_t i = 1; i < s->n; i++) {
        uint32_t d = ss_split_us(s, i);
        if (best == 0 || d < best) {
            best = d;
        }
    }
    return best;
}

uint32_t ss_avg_split_us(const shot_string_t *s)
{
    if (s->n < 2) {
        return 0;
    }
    return (ss_total_us(s) - ss_first_us(s)) / (s->n - 1);
}

bool ss_is_late(const shot_string_t *s, uint8_t i)
{
    if (s->par_count == 0 || i >= s->n) {
        return false;
    }
    return s->shots[i].t_us > s->par_ms[s->par_count - 1] * 1000u;
}

int ss_points(const uint8_t score[SCORE_FIELDS], uint8_t power_factor)
{
    // IPSC: Minor A5/C3/D1, Major A5/C4/D2; Miss und No-Shoot je -10
    int c = power_factor == PF_MAJOR ? 4 : 3;
    int d = power_factor == PF_MAJOR ? 2 : 1;
    return score[SCORE_A] * 5 + score[SCORE_C] * c + score[SCORE_D] * d -
           10 * (score[SCORE_MISS] + score[SCORE_NS]);
}

float ss_hit_factor(const shot_string_t *s)
{
    uint32_t total = ss_total_us(s);
    if (!s->scored || total == 0) {
        return 0.0f;
    }
    int pts = ss_points(s->score, s->power_factor);
    return pts > 0 ? pts / (total / 1e6f) : 0.0f;
}

void ss_format_time(char *buf, uint32_t t_us)
{
    uint32_t cs = (t_us + 5000) / 10000;   // Hundertstel, kaufmännisch gerundet
    snprintf(buf, 8, "%lu.%02lu", (unsigned long)(cs / 100), (unsigned long)(cs % 100));
}
