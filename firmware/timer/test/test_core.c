// Host-Tests für settings, shot_string und timer_core.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "settings.h"
#include "shot_string.h"
#include "timer_core.h"

static int s_failed, s_checks;

#define CHECK(cond)                                                             \
    do {                                                                        \
        s_checks++;                                                             \
        if (!(cond)) {                                                          \
            s_failed++;                                                         \
            printf("  FEHLER %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                       \
    } while (0)
#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        long long va_ = (long long)(a), vb_ = (long long)(b);                   \
        s_checks++;                                                             \
        if (va_ != vb_) {                                                       \
            s_failed++;                                                         \
            printf("  FEHLER %s:%d: %s == %lld, erwartet %lld\n", __FILE__,     \
                   __LINE__, #a, va_, vb_);                                     \
        }                                                                       \
    } while (0)
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)
#define RUN(fn) do { printf("%s\n", #fn); fn(); } while (0)

#define S(x) ((int64_t)((x) * 1000000.0 + 0.5))   // Sekunden -> µs

static uint32_t s_rand_value;
static uint32_t fake_rand(void) { return s_rand_value; }

// ---------- settings ----------

static void test_settings_defaults_valid(void)
{
    settings_t s;
    settings_defaults(&s);
    CHECK(!settings_sanitize(&s));
    CHECK_EQ(s.sensitivity, 6);
    CHECK_EQ(s.mode, MODE_STANDARD);
}

static void test_settings_clamp(void)
{
    settings_t s;
    settings_defaults(&s);
    s.sensitivity = 0;
    s.delay_min_ms = 5000;
    s.delay_max_ms = 2000;
    s.par_count = 42;
    s.par_ms[3] = 0;
    s.mode = 200;
    CHECK(settings_sanitize(&s));
    CHECK_EQ(s.sensitivity, 1);
    CHECK_EQ(s.delay_max_ms, 5000);
    CHECK_EQ(s.par_count, PAR_MAX);
    CHECK_EQ(s.par_ms[3], 100);
    CHECK_EQ(s.mode, MODE_COUNT - 1);
}

static void test_settings_version(void)
{
    settings_t s;
    settings_defaults(&s);
    s.version = 99;
    s.sensitivity = 9;
    CHECK(settings_sanitize(&s));
    CHECK_EQ(s.version, SETTINGS_VERSION);
    CHECK_EQ(s.sensitivity, 6);
}

static void test_threshold(void)
{
    CHECK(fabsf(settings_threshold_dbfs(1) - -6.0f) < 1e-4f);
    CHECK(fabsf(settings_threshold_dbfs(10) - -40.0f) < 1e-4f);
    for (uint8_t i = 1; i < 10; i++) {
        CHECK(settings_threshold_dbfs(i + 1) < settings_threshold_dbfs(i));
    }
    CHECK(settings_threshold_dbfs(0) == settings_threshold_dbfs(1));
}

static void test_active_pars(void)
{
    settings_t s;
    uint32_t p[PAR_MAX];
    settings_defaults(&s);
    s.par_count = 3;
    s.par_ms[0] = 3000;
    s.par_ms[1] = 1500;
    s.par_ms[2] = 2000;
    CHECK_EQ(settings_active_pars(&s, p), 0);       // Standard: keine Par-Zeiten
    s.mode = MODE_PAR;
    CHECK_EQ(settings_active_pars(&s, p), 3);
    CHECK_EQ(p[0], 1500);
    CHECK_EQ(p[1], 2000);
    CHECK_EQ(p[2], 3000);
    s.mode = MODE_REPEAT;
    s.par_in_repeat = false;
    CHECK_EQ(settings_active_pars(&s, p), 0);
    s.par_in_repeat = true;
    CHECK_EQ(settings_active_pars(&s, p), 3);
}

// ---------- shot_string ----------

static void make_string(shot_string_t *s, const double *t, int n)
{
    ss_clear(s);
    for (int i = 0; i < n; i++) {
        ss_add(s, (uint32_t)S(t[i]), -5);
    }
}

static void test_string_stats(void)
{
    const double t[] = { 1.18, 1.41, 1.62, 1.84 };
    shot_string_t s;
    make_string(&s, t, 4);
    CHECK_EQ(s.n, 4);
    CHECK_EQ(ss_first_us(&s), 1180000);
    CHECK_EQ(ss_total_us(&s), 1840000);
    CHECK_EQ(ss_split_us(&s, 0), 1180000);
    CHECK_EQ(ss_split_us(&s, 1), 230000);
    CHECK_EQ(ss_split_us(&s, 2), 210000);
    CHECK_EQ(ss_split_us(&s, 9), 0);
    CHECK_EQ(ss_fastest_split_us(&s), 210000);
    CHECK_EQ(ss_avg_split_us(&s), 220000);
}

static void test_string_empty(void)
{
    shot_string_t s;
    ss_clear(&s);
    CHECK_EQ(ss_first_us(&s), 0);
    CHECK_EQ(ss_total_us(&s), 0);
    CHECK_EQ(ss_fastest_split_us(&s), 0);
    CHECK_EQ(ss_avg_split_us(&s), 0);
    CHECK(!ss_delete_last(&s));
    CHECK(ss_hit_factor(&s) == 0.0f);
}

static void test_string_full_and_delete(void)
{
    shot_string_t s;
    ss_clear(&s);
    for (int i = 0; i < SHOTS_MAX; i++) {
        CHECK(ss_add(&s, (uint32_t)(i + 1) * 100000, -4));
    }
    CHECK(!ss_add(&s, 99999999, -4));
    CHECK_EQ(s.n, SHOTS_MAX);
    CHECK(ss_delete_last(&s));
    CHECK_EQ(s.n, SHOTS_MAX - 1);
    CHECK_EQ(ss_total_us(&s), 9800000);
}

static void test_string_late(void)
{
    const double t[] = { 1.0, 2.0, 2.0001 };
    shot_string_t s;
    make_string(&s, t, 3);
    CHECK(!ss_is_late(&s, 2));              // ohne Par nie spät
    s.par_count = 1;
    s.par_ms[0] = 2000;
    CHECK(!ss_is_late(&s, 0));
    CHECK(!ss_is_late(&s, 1));              // genau auf der Par-Zeit: nicht spät
    CHECK(ss_is_late(&s, 2));
}

static void test_scoring(void)
{
    uint8_t sc[SCORE_FIELDS] = { 4, 1, 1, 0, 0 };
    CHECK_EQ(ss_points(sc, PF_MINOR), 24);
    CHECK_EQ(ss_points(sc, PF_MAJOR), 26);
    uint8_t bad[SCORE_FIELDS] = { 0, 0, 1, 1, 1 };
    CHECK_EQ(ss_points(bad, PF_MINOR), -19);

    const double t[] = { 1.0, 1.5, 2.0, 2.5, 3.0, 4.0 };
    shot_string_t s;
    make_string(&s, t, 6);
    CHECK(ss_hit_factor(&s) == 0.0f);       // ohne Wertung
    s.scored = true;
    memcpy(s.score, sc, sizeof(sc));
    s.power_factor = PF_MINOR;
    CHECK(fabsf(ss_hit_factor(&s) - 6.0f) < 1e-4f);   // 24 Punkte / 4,00 s
    memcpy(s.score, bad, sizeof(bad));
    CHECK(ss_hit_factor(&s) == 0.0f);       // negative Punkte -> 0
}

static void test_format(void)
{
    char b[8];
    ss_format_time(b, 0);
    CHECK_STR(b, "0.00");
    ss_format_time(b, 4999);
    CHECK_STR(b, "0.00");
    ss_format_time(b, 5000);
    CHECK_STR(b, "0.01");
    ss_format_time(b, 12345678);
    CHECK_STR(b, "12.35");
    ss_format_time(b, 999990000);
    CHECK_STR(b, "999.99");
}

// ---------- timer_core ----------

static void base_settings(settings_t *s)
{
    settings_defaults(s);
    s->delay_type = DELAY_FIXED;
    s->delay_fixed_ms = 2000;
    s->auto_stop_s = 5;
}

static void test_core_standard(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    tc_init(&tc, fake_rand, 7);

    uint32_t a = tc_start_key(&tc, &s, S(100));
    CHECK_EQ(tc.state, TC_DELAY);
    CHECK(a & TC_ACT_CHANGED);
    CHECK_EQ(tc_next_deadline(&tc), S(102));
    CHECK_EQ(tc_tick(&tc, S(101.999)), 0);

    a = tc_tick(&tc, S(102.0));
    CHECK_EQ(tc.state, TC_RUN);
    CHECK(a & TC_ACT_BEEP_START);
    CHECK(a & TC_ACT_DETECT_ON);
    CHECK_EQ(tc.run.delay_ms, 2000);

    CHECK(tc_shot(&tc, S(103.25), -4) & TC_ACT_SHOT_ADDED);
    CHECK(tc_shot(&tc, S(103.50), -5) & TC_ACT_SHOT_ADDED);
    CHECK_EQ(tc_elapsed_us(&tc, S(104)), S(2));

    a = tc_start_key(&tc, &s, S(104));
    CHECK(a & TC_ACT_STRING_DONE);
    CHECK(a & TC_ACT_DETECT_OFF);
    CHECK_EQ(tc.state, TC_READY);
    CHECK(tc.has_last);
    CHECK_EQ(tc.last.seq, 7);
    CHECK_EQ(tc.seq, 8);
    CHECK_EQ(tc.last.n, 2);
    CHECK_EQ(tc.last.shots[0].t_us, 1250000);
    CHECK_EQ(tc.last.shots[1].t_us, 1500000);
    CHECK_EQ(tc.last.par_count, 0);
    CHECK_EQ(tc.end_reason, TC_END_KEY);
}

static void test_core_random_delay(void)
{
    timer_core_t tc;
    settings_t s;
    settings_defaults(&s);              // Zufall 1,0 - 4,0 s
    tc_init(&tc, fake_rand, 1);
    s_rand_value = 0;
    tc_start_key(&tc, &s, S(10));
    CHECK_EQ(tc.delay_end_us, S(11));
    tc_start_key(&tc, &s, S(10));       // Abbruch
    s_rand_value = 3000;                // 3000 % 3001 = 3000 -> 4,0 s
    tc_start_key(&tc, &s, S(10));
    CHECK_EQ(tc.delay_end_us, S(14));
    tc_start_key(&tc, &s, S(10));
    s_rand_value = 3001;                // 3001 % 3001 = 0 -> 1,0 s
    tc_start_key(&tc, &s, S(10));
    CHECK_EQ(tc.delay_end_us, S(11));

    s.delay_type = DELAY_INSTANT;
    tc_start_key(&tc, &s, S(10));       // Abbruch
    tc_start_key(&tc, &s, S(20));
    CHECK(tc_tick(&tc, S(20)) & TC_ACT_BEEP_START);
}

static void test_core_abort_and_ignored_shots(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    tc_init(&tc, fake_rand, 1);

    CHECK_EQ(tc_shot(&tc, S(1), -4), 0);    // im Bereit-Zustand
    tc_start_key(&tc, &s, S(0));
    CHECK_EQ(tc_shot(&tc, S(1), -4), 0);    // während der Verzögerung
    uint32_t a = tc_start_key(&tc, &s, S(1));
    CHECK_EQ(tc.state, TC_READY);
    CHECK(!(a & TC_ACT_STRING_DONE));
    CHECK(!tc.has_last);

    tc_start_key(&tc, &s, S(10));
    tc_tick(&tc, S(12));
    CHECK_EQ(tc_shot(&tc, S(11.9), -4), 0); // vor dem Beep
    CHECK_EQ(tc.run.n, 0);
}

static void test_core_settings_snapshot(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    tc_init(&tc, fake_rand, 1);
    tc_start_key(&tc, &s, S(0));
    s.delay_fixed_ms = 9000;
    s.mode = MODE_DRY;
    tc_tick(&tc, S(2));
    CHECK_EQ(tc.state, TC_RUN);
    CHECK(tc_shot(&tc, S(3), -4) & TC_ACT_SHOT_ADDED);   // läuft weiter als Standard
}

static void test_core_par(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    s.mode = MODE_PAR;
    s.par_count = 3;
    s.par_ms[0] = 3000;
    s.par_ms[1] = 1500;
    s.par_ms[2] = 1600;
    tc_init(&tc, fake_rand, 1);
    tc_start_key(&tc, &s, S(0));
    tc_tick(&tc, S(2));                     // Beep bei t0 = 2 s
    CHECK_EQ(tc.run.par_count, 3);
    CHECK_EQ(tc.run.par_ms[0], 1500);
    CHECK_EQ(tc_next_deadline(&tc), S(3.5));

    CHECK(!(tc_tick(&tc, S(3.49)) & TC_ACT_BEEP_PAR));
    CHECK(tc_tick(&tc, S(3.5)) & TC_ACT_BEEP_PAR);
    CHECK_EQ(tc.par_idx, 1);
    CHECK(tc_tick(&tc, S(3.7)) & TC_ACT_BEEP_PAR);   // 1,6 s nachgeholt
    CHECK_EQ(tc.par_idx, 2);
    CHECK_EQ(tc_next_deadline(&tc), S(5.0));
    CHECK(tc_tick(&tc, S(5.0)) & TC_ACT_BEEP_PAR);
    CHECK_EQ(tc.par_idx, 3);

    // Auto-Stopp zählt ab der letzten Par-Zeit (3,0 s), solange kein späterer Schuss fällt
    CHECK_EQ(tc_next_deadline(&tc), S(2 + 3.0 + 5));
    tc_shot(&tc, S(2 + 3.4), -5);
    CHECK_EQ(tc_next_deadline(&tc), S(2 + 3.4 + 5));
    CHECK(!(tc_tick(&tc, S(2 + 8.3)) & TC_ACT_STRING_DONE));
    CHECK(tc_tick(&tc, S(2 + 8.4)) & TC_ACT_STRING_DONE);
    CHECK_EQ(tc.end_reason, TC_END_AUTO_STOP);
    CHECK(ss_is_late(&tc.last, 0));
}

static void test_core_auto_stop_without_shots(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    tc_init(&tc, fake_rand, 1);
    tc_start_key(&tc, &s, S(0));
    tc_tick(&tc, S(2));
    CHECK_EQ(tc_next_deadline(&tc), S(7));
    uint32_t a = tc_tick(&tc, S(7));
    CHECK(a & TC_ACT_STRING_DONE);
    CHECK_EQ(tc.last.n, 0);
}

static void test_core_spy(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    s.mode = MODE_SPY;
    tc_init(&tc, fake_rand, 1);
    uint32_t a = tc_start_key(&tc, &s, S(50));
    CHECK_EQ(tc.state, TC_RUN);
    CHECK(!(a & TC_ACT_BEEP_START));
    CHECK(a & TC_ACT_DETECT_ON);
    tc_shot(&tc, S(51.2), -10);
    tc_start_key(&tc, &s, S(52));
    CHECK_EQ(tc.last.shots[0].t_us, 1200000);
    CHECK_EQ(tc.last.delay_ms, 0);
}

static void test_core_dry(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    s.mode = MODE_DRY;
    s.par_ms[0] = 1200;
    s.dry_reps = 3;
    s.repeat_pause_s = 4;
    tc_init(&tc, fake_rand, 1);

    tc_start_key(&tc, &s, S(0));
    uint32_t a = tc_tick(&tc, S(2));
    CHECK(a & TC_ACT_BEEP_START);
    CHECK(!(a & TC_ACT_DETECT_ON));
    CHECK_EQ(tc_shot(&tc, S(2.5), -3), 0);
    CHECK(tc_tick(&tc, S(3.2)) & TC_ACT_BEEP_PAR);
    CHECK_EQ(tc_next_deadline(&tc), S(3.8));
    a = tc_tick(&tc, S(3.8));
    CHECK(a & TC_ACT_STRING_DONE);
    CHECK_EQ(tc.end_reason, TC_END_DRY);
    CHECK_EQ(tc.state, TC_PAUSE);                   // Durchgang 2 von 3 folgt
    CHECK_EQ(tc_next_deadline(&tc), S(7.8));
    tc_tick(&tc, S(7.8));
    CHECK_EQ(tc.state, TC_DELAY);
    tc_tick(&tc, S(9.8));
    tc_tick(&tc, S(11.6));                          // Ende Durchgang 2
    CHECK_EQ(tc.state, TC_PAUSE);
    tc_tick(&tc, S(15.6));
    tc_tick(&tc, S(17.6));
    tc_tick(&tc, S(19.4));                          // Ende Durchgang 3
    CHECK_EQ(tc.state, TC_READY);
    CHECK_EQ(tc.seq, 4);

    // START in der Pause beendet die Serie
    tc_start_key(&tc, &s, S(100));
    tc_tick(&tc, S(102));
    tc_tick(&tc, S(103.8));
    CHECK_EQ(tc.state, TC_PAUSE);
    tc_start_key(&tc, &s, S(104));
    CHECK_EQ(tc.state, TC_READY);
}

static void test_core_repeat(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    s.mode = MODE_REPEAT;
    s.par_in_repeat = false;
    tc_init(&tc, fake_rand, 1);
    tc_start_key(&tc, &s, S(0));
    tc_tick(&tc, S(2));
    tc_shot(&tc, S(3), -4);
    tc_tick(&tc, S(8));                     // Auto-Stopp
    CHECK_EQ(tc.state, TC_PAUSE);
    tc_tick(&tc, S(13));
    CHECK_EQ(tc.state, TC_DELAY);
    tc_tick(&tc, S(15));
    CHECK_EQ(tc.state, TC_RUN);
    tc_start_key(&tc, &s, S(16));           // START beendet die Serie ohne Pause
    CHECK_EQ(tc.state, TC_READY);
}

static void test_core_countdown(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    s.mode = MODE_COUNTDOWN;
    s.countdown_s = 30;
    s.countdown_warn_s = 10;
    tc_init(&tc, fake_rand, 1);
    tc_start_key(&tc, &s, S(0));
    tc_tick(&tc, S(2));
    CHECK_EQ(tc_next_deadline(&tc), S(22));
    tc_shot(&tc, S(5), -4);
    CHECK(!(tc_tick(&tc, S(21.9)) & TC_ACT_BEEP_WARN));
    CHECK(tc_tick(&tc, S(22)) & TC_ACT_BEEP_WARN);
    CHECK(!(tc_tick(&tc, S(23)) & TC_ACT_BEEP_WARN));   // nur einmal
    CHECK_EQ(tc_next_deadline(&tc), S(32));
    CHECK_EQ(tc.state, TC_RUN);                         // kein Auto-Stopp im Countdown
    uint32_t a = tc_tick(&tc, S(32));
    CHECK(a & TC_ACT_BEEP_END);
    CHECK(a & TC_ACT_STRING_DONE);
    CHECK_EQ(tc.end_reason, TC_END_COUNTDOWN);
    CHECK_EQ(tc.last.n, 1);
}

static void test_core_max_time_and_full(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    s.auto_stop_s = 0;
    tc_init(&tc, fake_rand, 1);
    tc_start_key(&tc, &s, S(0));
    tc_tick(&tc, S(2));
    CHECK_EQ(tc_next_deadline(&tc), S(2) + TC_MAX_TIME_US);
    CHECK(tc_tick(&tc, S(2) + TC_MAX_TIME_US) & TC_ACT_STRING_DONE);
    CHECK_EQ(tc.end_reason, TC_END_MAX_TIME);

    tc_start_key(&tc, &s, S(2000));
    tc_tick(&tc, S(2002));
    for (int i = 0; i < SHOTS_MAX; i++) {
        tc_shot(&tc, S(2003) + i * 100000, -4);
    }
    CHECK_EQ(tc.state, TC_RUN);
    CHECK(tc_shot(&tc, S(2100), -4) & TC_ACT_STRING_DONE);
    CHECK_EQ(tc.end_reason, TC_END_FULL);
    CHECK_EQ(tc.last.n, SHOTS_MAX);
}

static void test_core_set_t0(void)
{
    timer_core_t tc;
    settings_t s;
    base_settings(&s);
    tc_init(&tc, fake_rand, 1);
    tc_start_key(&tc, &s, S(0));
    tc_tick(&tc, S(2.0));
    tc_set_t0(&tc, S(2.0003));          // Beep begann 0,3 ms später
    tc_shot(&tc, S(3.0003), -4);
    CHECK_EQ(tc.run.shots[0].t_us, 1000000);
}

int main(void)
{
    RUN(test_settings_defaults_valid);
    RUN(test_settings_clamp);
    RUN(test_settings_version);
    RUN(test_threshold);
    RUN(test_active_pars);
    RUN(test_string_stats);
    RUN(test_string_empty);
    RUN(test_string_full_and_delete);
    RUN(test_string_late);
    RUN(test_scoring);
    RUN(test_format);
    RUN(test_core_standard);
    RUN(test_core_random_delay);
    RUN(test_core_abort_and_ignored_shots);
    RUN(test_core_settings_snapshot);
    RUN(test_core_par);
    RUN(test_core_auto_stop_without_shots);
    RUN(test_core_spy);
    RUN(test_core_dry);
    RUN(test_core_repeat);
    RUN(test_core_countdown);
    RUN(test_core_max_time_and_full);
    RUN(test_core_set_t0);
    printf("\n%d Prüfungen, %d Fehler\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
