// Host-Tests für die dauerhafte Historie (Ring, CRC, Stromausfall, Aktualisieren, Löschen).
#include <stdio.h>
#include <string.h>

#include "history.h"

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

// Simulierter Flash: gelöscht = 0xff; Fehlerinjektion für abgebrochene Schreibvorgänge
static uint8_t s_mem[HIST_SLOTS][HIST_REC_SIZE];
static int s_fail_after = -1;   // >= 0: nach so vielen Bytes Schreibvorgang abbrechen
static int s_writes;

static bool mem_read(void *ctx, uint16_t slot, uint8_t *buf)
{
    (void)ctx;
    memcpy(buf, s_mem[slot], HIST_REC_SIZE);
    return true;
}

static bool mem_write(void *ctx, uint16_t slot, const uint8_t *buf)
{
    (void)ctx;
    s_writes++;
    memset(s_mem[slot], 0xff, HIST_REC_SIZE);
    if (s_fail_after >= 0) {
        memcpy(s_mem[slot], buf, (size_t)s_fail_after);
        s_fail_after = -1;
        return false;
    }
    memcpy(s_mem[slot], buf, HIST_REC_SIZE);
    return true;
}

static bool mem_erase_all(void *ctx)
{
    (void)ctx;
    memset(s_mem, 0xff, sizeof(s_mem));
    return true;
}

static const hist_io_t IO = { mem_read, mem_write, mem_erase_all, NULL };
static history_t h;

static void fresh(void)
{
    mem_erase_all(NULL);
    s_fail_after = -1;
    s_writes = 0;
    hist_open(&h, &IO);
}

static shot_string_t make(uint32_t seq, uint8_t n)
{
    shot_string_t s;
    ss_clear(&s);
    s.seq = seq;
    s.mode = MODE_PAR;
    s.par_count = 2;
    s.par_ms[0] = 1500;
    s.par_ms[1] = 3000;
    s.delay_ms = 2345;
    for (uint8_t i = 0; i < n; i++) {
        ss_add(&s, 1000000u + i * 250000u + seq, (int8_t)(-3 - i));
    }
    return s;
}

static bool same(const shot_string_t *a, const shot_string_t *b)
{
    if (a->seq != b->seq || a->mode != b->mode || a->n != b->n || a->par_count != b->par_count ||
        a->delay_ms != b->delay_ms || a->scored != b->scored || a->power_factor != b->power_factor ||
        memcmp(a->score, b->score, SCORE_FIELDS) || memcmp(a->par_ms, b->par_ms, sizeof(a->par_ms))) {
        return false;
    }
    for (int i = 0; i < a->n; i++) {
        if (a->shots[i].t_us != b->shots[i].t_us || a->shots[i].db != b->shots[i].db) {
            return false;
        }
    }
    return true;
}

static void test_pack_roundtrip(void)
{
    static uint8_t buf[HIST_REC_SIZE];
    shot_string_t a = make(42, SHOTS_MAX), b;
    a.scored = true;
    a.power_factor = PF_MAJOR;
    a.score[SCORE_A] = 90;
    a.score[SCORE_NS] = 2;
    hist_pack(&a, buf);
    CHECK(hist_unpack(buf, &b));
    CHECK(same(&a, &b));
    buf[100] ^= 0x01;                       // ein Bit kippen
    CHECK(!hist_unpack(buf, &b));
    memset(buf, 0xff, sizeof(buf));         // gelöschter Block
    CHECK(!hist_unpack(buf, &b));
    CHECK_EQ(hist_crc32((const uint8_t *)"123456789", 9), 0xcbf43926u);    // Prüfwert CRC-32
}

static void test_empty(void)
{
    fresh();
    shot_string_t s;
    CHECK_EQ(h.count, 0);
    CHECK_EQ(hist_next_seq(&h), 1);
    CHECK(!hist_get(&h, 0, &s));
}

static void test_append_and_reopen(void)
{
    fresh();
    for (uint32_t i = 1; i <= 5; i++) {
        shot_string_t s = make(i, (uint8_t)i);
        CHECK(hist_append(&h, &s));
    }
    hist_open(&h, &IO);                     // Neustart
    CHECK_EQ(h.count, 5);
    CHECK_EQ(hist_next_seq(&h), 6);
    shot_string_t s, e = make(5, 5);
    CHECK(hist_get(&h, 0, &s));
    CHECK(same(&s, &e));
    CHECK(hist_get(&h, 4, &s));
    CHECK_EQ(s.seq, 1);
}

static void test_ring_wraps(void)
{
    fresh();
    for (uint32_t i = 1; i <= HIST_SLOTS + 37; i++) {
        shot_string_t s = make(i, 2);
        hist_append(&h, &s);
    }
    CHECK_EQ(h.count, HIST_SLOTS);
    hist_open(&h, &IO);
    CHECK_EQ(h.count, HIST_SLOTS);
    shot_string_t s;
    CHECK(hist_get(&h, 0, &s));
    CHECK_EQ(s.seq, HIST_SLOTS + 37);
    CHECK(hist_get(&h, HIST_SLOTS - 1, &s));
    CHECK_EQ(s.seq, 38);                    // die 37 ältesten sind überschrieben
    CHECK_EQ(hist_next_seq(&h), HIST_SLOTS + 38);
}

static void test_power_loss_during_append(void)
{
    fresh();
    for (uint32_t i = 1; i <= 3; i++) {
        shot_string_t s = make(i, 3);
        hist_append(&h, &s);
    }
    shot_string_t s = make(4, 3);
    s_fail_after = 300;                     // Strom weg mitten im Schreiben
    CHECK(!hist_append(&h, &s));
    hist_open(&h, &IO);
    CHECK_EQ(h.count, 3);                   // nur der abgebrochene String fehlt
    CHECK(hist_get(&h, 0, &s));
    CHECK_EQ(s.seq, 3);
    // danach geht es normal weiter
    s = make(hist_next_seq(&h), 1);
    CHECK(hist_append(&h, &s));
    CHECK_EQ(h.count, 4);
}

static void test_corrupt_block_in_middle(void)
{
    fresh();
    for (uint32_t i = 1; i <= 6; i++) {
        shot_string_t s = make(i, 2);
        hist_append(&h, &s);
    }
    s_mem[2][50] ^= 0x80;                   // Datensatz 3 beschädigt
    hist_open(&h, &IO);
    CHECK_EQ(h.count, 5);
    shot_string_t s;
    for (uint16_t i = 0; i < h.count; i++) {
        CHECK(hist_get(&h, i, &s));
        CHECK(s.seq != 3);
    }
    // Anhängen nach dem neuesten, unabhängig von der Lücke
    s = make(7, 1);
    hist_append(&h, &s);
    CHECK(hist_get(&h, 0, &s));
    CHECK_EQ(s.seq, 7);
    CHECK_EQ(h.count, 6);
}

static void test_update(void)
{
    fresh();
    for (uint32_t i = 1; i <= 4; i++) {
        shot_string_t s = make(i, 4);
        hist_append(&h, &s);
    }
    shot_string_t s;
    CHECK(hist_get(&h, 1, &s));             // seq 3
    s.scored = true;
    s.score[SCORE_A] = 3;
    ss_delete_last(&s);
    int before = s_writes;
    CHECK(hist_update(&h, &s));
    CHECK_EQ(s_writes, before + 1);
    hist_open(&h, &IO);
    shot_string_t r;
    CHECK(hist_get(&h, 1, &r));
    CHECK(same(&r, &s));
    CHECK_EQ(h.count, 4);
    shot_string_t unknown = make(99, 1);
    CHECK(!hist_update(&h, &unknown));
}

static void test_clear(void)
{
    fresh();
    for (uint32_t i = 1; i <= 10; i++) {
        shot_string_t s = make(i, 1);
        hist_append(&h, &s);
    }
    CHECK(hist_clear(&h));
    CHECK_EQ(h.count, 0);
    CHECK_EQ(hist_next_seq(&h), 11);        // Nummern laufen weiter
    shot_string_t s = make(11, 2);
    CHECK(hist_append(&h, &s));
    hist_open(&h, &IO);
    CHECK_EQ(h.count, 1);
}

int main(void)
{
    RUN(test_pack_roundtrip);
    RUN(test_empty);
    RUN(test_append_and_reopen);
    RUN(test_ring_wraps);
    RUN(test_power_loss_during_append);
    RUN(test_corrupt_block_in_middle);
    RUN(test_update);
    RUN(test_clear);
    printf("\n%d Prüfungen, %d Fehler\n", s_checks, s_failed);
    return s_failed ? 1 : 0;
}
