// Dauerhafte Historie: Ring aus HIST_SLOTS Datensätzen fester Größe mit CRC32.
// Jeder Datensatz liegt in einem eigenen Speicherblock (auf dem Gerät ein 4-KB-Flash-Sektor),
// sodass ein Stromausfall beim Schreiben höchstens diesen einen Datensatz ungültig macht.
// Plattformunabhängig: Lesen und Schreiben über hist_io_t (Gerät: Flash-Partition, PC: RAM).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "shot_string.h"

#define HIST_SLOTS      200
#define HIST_REC_SIZE   1024
#define HIST_MAGIC      0x4853u     // "SH"
#define HIST_VERSION    1

typedef struct {
    bool (*read)(void *ctx, uint16_t slot, uint8_t *buf);           // HIST_REC_SIZE Byte lesen
    bool (*write)(void *ctx, uint16_t slot, const uint8_t *buf);    // Block löschen, dann schreiben
    bool (*erase_all)(void *ctx);                                   // alle Blöcke löschen
    void *ctx;
} hist_io_t;

typedef struct {
    hist_io_t io;
    uint16_t count;                     // gültige Datensätze
    uint16_t order[HIST_SLOTS];         // Blocknummern, neuester zuerst
    uint32_t slot_seq[HIST_SLOTS];      // seq je Block, 0 = leer/ungültig
    uint32_t max_seq;
    uint8_t buf[HIST_REC_SIZE];
} history_t;

// Liest alle Blöcke und baut den Index auf. Ungültige Blöcke (leer, CRC falsch) werden übersprungen.
void hist_open(history_t *h, const hist_io_t *io);

uint32_t hist_next_seq(const history_t *h);     // Nummer für den nächsten String
bool hist_append(history_t *h, const shot_string_t *s);
bool hist_update(history_t *h, const shot_string_t *s);   // Datensatz mit gleicher seq ersetzen
bool hist_get(history_t *h, uint16_t idx, shot_string_t *out);  // idx 0 = neuester
bool hist_clear(history_t *h);

// Serialisierung (für Tests offengelegt)
void hist_pack(const shot_string_t *s, uint8_t buf[HIST_REC_SIZE]);
bool hist_unpack(const uint8_t buf[HIST_REC_SIZE], shot_string_t *out);
uint32_t hist_crc32(const uint8_t *data, uint32_t len);
