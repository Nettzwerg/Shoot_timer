// Host-Tests für die Energieverwaltung (Akkuzustand, Dimmen, Auto-Aus).
#include <stdio.h>

#include "power.h"

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
#define RUN(fn) do { printf("%s\n", #fn); fn(); } while (0)
#define S(x) ((int64_t)((x) * 1000000.0))

static power_t p;

// Spannung n Sekunden lang einmal pro Sekunde melden, Aktions-Bits sammeln
static uint32_t feed(float v, int seconds, int64_t *t)
{
    uint32_t act = 0;
    for (int i = 0; i < seconds; i++) {
        act |= pwr_battery(&p, v, *t);
        *t += S(1);
    }
    return act;
}

static void test_percent(void)
{
    CHECK_EQ(pwr_percent(4.25f), 100);
    CHECK_EQ(pwr_percent(4.20f), 100);
    CHECK_EQ(pwr_percent(3.80f), 50);
    CHECK(pwr_percent(3.85f) >= 57 && pwr_percent(3.85f) <= 58);  // Mitte zwischen 50 und 65
    CHECK_EQ(pwr_percent(3.30f), 0);
    CHECK_EQ(pwr_percent(2.0f), 0);
    for (float v = 3.3f; v < 4.2f; v += 0.01f) {
        CHECK(pwr_percent(v + 0.01f) >= pwr_percent(v));    // monoton
    }
}

static void test_no_battery(void)
{
    int64_t t = 0;
    pwr_init(&p, t);
    uint32_t act = feed(0.05f, 30, &t);     // Eingang auf GND (USB-Betrieb)
    CHECK_EQ(p.state, BATT_NONE);
    CHECK_EQ(act, 0);                       // weder Warnung noch Abschalten
    feed(3.95f, 1, &t);                     // Akku eingesteckt: sofort gültiger Wert
    CHECK_EQ(p.state, BATT_OK);
    CHECK(p.v > 3.9f);
}

static void test_low_warning_with_hysteresis(void)
{
    int64_t t = 0;
    pwr_init(&p, t);
    CHECK_EQ(feed(3.70f, 5, &t), 0);
    CHECK_EQ(p.state, BATT_OK);
    uint32_t act = feed(3.38f, 20, &t);
    CHECK(act & PWR_ACT_LOW_WARN);
    CHECK_EQ(p.state, BATT_LOW);
    CHECK_EQ(feed(3.38f, 20, &t) & PWR_ACT_LOW_WARN, 0);    // nur einmal
    feed(3.45f, 30, &t);                    // knapp über der Warnschwelle: bleibt LOW
    CHECK_EQ(p.state, BATT_LOW);
    feed(3.60f, 30, &t);                    // laden: über der Hysterese
    CHECK_EQ(p.state, BATT_OK);
    CHECK(feed(3.35f, 30, &t) & PWR_ACT_LOW_WARN);          // erneut schwach: neue Warnung
}

static void test_critical_needs_hold_time(void)
{
    int64_t t = 0;
    pwr_init(&p, t);
    feed(3.31f, 3, &t);
    // kurzer Einbruch (z. B. beim Beep) löst nichts aus
    uint32_t act = feed(3.10f, 3, &t);
    act |= feed(3.50f, 30, &t);
    CHECK_EQ(act & PWR_ACT_SHUTDOWN, 0);
    // dauerhaft zu niedrig: nach 10 s abschalten, genau einmal
    act = feed(3.20f, 25, &t);
    CHECK(act & PWR_ACT_SHUTDOWN);
    CHECK_EQ(p.state, BATT_CRITICAL);
    CHECK_EQ(feed(3.20f, 20, &t) & PWR_ACT_SHUTDOWN, 0);
}

static void test_dim_and_auto_off(void)
{
    pwr_init(&p, 0);
    CHECK_EQ(pwr_tick(&p, S(59), false, 15), 0);
    CHECK_EQ(pwr_tick(&p, S(60), false, 15), PWR_ACT_DIM);
    CHECK_EQ(pwr_tick(&p, S(61), false, 15), 0);           // nur einmal dimmen
    CHECK_EQ(pwr_activity(&p, S(70)), PWR_ACT_UNDIM);
    CHECK_EQ(pwr_activity(&p, S(71)), 0);
    CHECK_EQ(pwr_tick(&p, S(71 + 15 * 60 - 1), false, 15) & PWR_ACT_OFF, 0);
    CHECK(pwr_tick(&p, S(71 + 15 * 60), false, 15) & PWR_ACT_OFF);
}

static void test_busy_keeps_awake(void)
{
    pwr_init(&p, 0);
    for (int s = 0; s <= 3600; s += 10) {
        CHECK_EQ(pwr_tick(&p, S(s), true, 5), 0);          // eine Stunde Lauf/Live-Anzeige
    }
    CHECK_EQ(pwr_tick(&p, S(3600 + 59), false, 5), 0);     // Zählung beginnt erst danach
    CHECK_EQ(pwr_tick(&p, S(3600 + 60), false, 5), PWR_ACT_DIM);
}

int main(void)
{
    RUN(test_percent);
    RUN(test_no_battery);
    RUN(test_low_warning_with_hysteresis);
    RUN(test_critical_needs_hold_time);
    RUN(test_dim_and_auto_off);
    RUN(test_busy_keeps_awake);
    printf("\n%d Prüfungen, %d Fehler\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
