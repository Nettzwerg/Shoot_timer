#include "history.h"

#include <string.h>

#define CRC_OFFSET  (HIST_REC_SIZE - 4)

uint32_t hist_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xffffffffu;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

// Feste Byte-Reihenfolge (little endian), unabhängig vom Compiler-Layout der Struktur
static uint8_t *put16(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xff;
    p[1] = v >> 8;
    return p + 2;
}

static uint8_t *put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; i++) {
        p[i] = (v >> (8 * i)) & 0xff;
    }
    return p + 4;
}

static uint16_t get16(const uint8_t **p)
{
    uint16_t v = (*p)[0] | ((*p)[1] << 8);
    *p += 2;
    return v;
}

static uint32_t get32(const uint8_t **p)
{
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        v |= (uint32_t)(*p)[i] << (8 * i);
    }
    *p += 4;
    return v;
}

void hist_pack(const shot_string_t *s, uint8_t buf[HIST_REC_SIZE])
{
    memset(buf, 0, HIST_REC_SIZE);
    uint8_t *p = buf;
    p = put16(p, HIST_MAGIC);
    p = put16(p, HIST_VERSION);
    p = put32(p, s->seq);
    *p++ = s->mode;
    *p++ = s->n;
    *p++ = s->par_count;
    *p++ = s->scored;
    *p++ = s->power_factor;
    for (int i = 0; i < SCORE_FIELDS; i++) {
        *p++ = s->score[i];
    }
    p = put32(p, s->delay_ms);
    for (int i = 0; i < PAR_MAX; i++) {
        p = put32(p, s->par_ms[i]);
    }
    for (int i = 0; i < s->n && i < SHOTS_MAX; i++) {
        p = put32(p, s->shots[i].t_us);
        *p++ = (uint8_t)s->shots[i].db;
    }
    put32(buf + CRC_OFFSET, hist_crc32(buf, CRC_OFFSET));
}

bool hist_unpack(const uint8_t buf[HIST_REC_SIZE], shot_string_t *out)
{
    const uint8_t *p = buf + CRC_OFFSET;
    if (get32(&p) != hist_crc32(buf, CRC_OFFSET)) {
        return false;
    }
    p = buf;
    if (get16(&p) != HIST_MAGIC || get16(&p) != HIST_VERSION) {
        return false;
    }
    shot_string_t s;
    memset(&s, 0, sizeof(s));
    s.seq = get32(&p);
    s.mode = *p++;
    s.n = *p++;
    s.par_count = *p++;
    s.scored = *p++ != 0;
    s.power_factor = *p++;
    for (int i = 0; i < SCORE_FIELDS; i++) {
        s.score[i] = *p++;
    }
    s.delay_ms = get32(&p);
    for (int i = 0; i < PAR_MAX; i++) {
        s.par_ms[i] = get32(&p);
    }
    if (s.seq == 0 || s.n > SHOTS_MAX || s.par_count > PAR_MAX || s.mode >= MODE_COUNT) {
        return false;
    }
    for (int i = 0; i < s.n; i++) {
        s.shots[i].t_us = get32(&p);
        s.shots[i].db = (int8_t)*p++;
    }
    *out = s;
    return true;
}

// order[] nach seq absteigend einsortieren
static void index_insert(history_t *h, uint16_t slot)
{
    uint16_t i = h->count;
    while (i > 0 && h->slot_seq[h->order[i - 1]] < h->slot_seq[slot]) {
        h->order[i] = h->order[i - 1];
        i--;
    }
    h->order[i] = slot;
    h->count++;
}

static void index_remove(history_t *h, uint16_t slot)
{
    for (uint16_t i = 0; i < h->count; i++) {
        if (h->order[i] == slot) {
            memmove(&h->order[i], &h->order[i + 1], (h->count - i - 1) * sizeof(h->order[0]));
            h->count--;
            break;
        }
    }
    h->slot_seq[slot] = 0;
}

void hist_open(history_t *h, const hist_io_t *io)
{
    memset(h, 0, sizeof(*h));
    h->io = *io;
    shot_string_t s;
    for (uint16_t slot = 0; slot < HIST_SLOTS; slot++) {
        if (h->io.read(h->io.ctx, slot, h->buf) && hist_unpack(h->buf, &s)) {
            h->slot_seq[slot] = s.seq;
            if (s.seq > h->max_seq) {
                h->max_seq = s.seq;
            }
            index_insert(h, slot);
        }
    }
}

uint32_t hist_next_seq(const history_t *h)
{
    return h->max_seq + 1;
}

bool hist_append(history_t *h, const shot_string_t *s)
{
    // Nächster Block nach dem neuesten; bei vollem Ring ist das der älteste
    uint16_t slot = h->count ? (h->order[0] + 1) % HIST_SLOTS : 0;
    if (h->slot_seq[slot]) {
        index_remove(h, slot);
    }
    hist_pack(s, h->buf);
    if (!h->io.write(h->io.ctx, slot, h->buf)) {
        return false;
    }
    h->slot_seq[slot] = s->seq;
    if (s->seq > h->max_seq) {
        h->max_seq = s->seq;
    }
    index_insert(h, slot);
    return true;
}

bool hist_update(history_t *h, const shot_string_t *s)
{
    for (uint16_t i = 0; i < h->count; i++) {
        uint16_t slot = h->order[i];
        if (h->slot_seq[slot] == s->seq) {
            hist_pack(s, h->buf);
            return h->io.write(h->io.ctx, slot, h->buf);
        }
    }
    return false;
}

bool hist_get(history_t *h, uint16_t idx, shot_string_t *out)
{
    if (idx >= h->count) {
        return false;
    }
    return h->io.read(h->io.ctx, h->order[idx], h->buf) && hist_unpack(h->buf, out);
}

bool hist_clear(history_t *h)
{
    bool ok = h->io.erase_all(h->io.ctx);
    // seq läuft bis zum nächsten Neustart weiter (danach beginnt die Zählung bei leerer Historie neu)
    uint32_t keep = h->max_seq;
    hist_io_t io = h->io;
    memset(h, 0, sizeof(*h));
    h->io = io;
    h->max_seq = keep;
    return ok;
}
