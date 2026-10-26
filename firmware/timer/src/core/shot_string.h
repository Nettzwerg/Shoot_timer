// Ein String (Durchgang): Schüsse, Splits, Statistik, Wertung.
// Plattformunabhängig, wird auf dem PC getestet.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

#define SHOTS_MAX       99
#define SCORE_FIELDS    5       // A, C, D, Miss, No-Shoot

enum { SCORE_A = 0, SCORE_C, SCORE_D, SCORE_MISS, SCORE_NS };

typedef struct {
    uint32_t t_us;              // Zeit ab Start-Beep
    int8_t db;                  // Spitzenpegel in dBFS
} shot_t;

typedef struct {
    uint32_t seq;               // fortlaufende Nummer (Historie)
    uint8_t mode;               // timer_mode_t
    uint8_t n;                  // Anzahl Schüsse
    uint8_t par_count;
    uint32_t par_ms[PAR_MAX];   // aufsteigend
    uint32_t delay_ms;          // tatsächliche Startverzögerung
    shot_t shots[SHOTS_MAX];
    bool scored;
    uint8_t score[SCORE_FIELDS];
    uint8_t power_factor;       // power_factor_t
} shot_string_t;

void ss_clear(shot_string_t *s);
bool ss_add(shot_string_t *s, uint32_t t_us, int8_t db);    // false, wenn voll
bool ss_delete_last(shot_string_t *s);

uint32_t ss_first_us(const shot_string_t *s);               // 0 ohne Schüsse
uint32_t ss_total_us(const shot_string_t *s);               // Zeit des letzten Schusses
uint32_t ss_split_us(const shot_string_t *s, uint8_t i);    // Schuss i zum vorherigen; i = 0: erster Schuss
uint32_t ss_fastest_split_us(const shot_string_t *s);       // 0 bei weniger als 2 Schüssen
uint32_t ss_avg_split_us(const shot_string_t *s);           // 0 bei weniger als 2 Schüssen
bool ss_is_late(const shot_string_t *s, uint8_t i);         // nach der letzten Par-Zeit

int ss_points(const uint8_t score[SCORE_FIELDS], uint8_t power_factor);
float ss_hit_factor(const shot_string_t *s);                // 0 ohne Wertung oder ohne Schüsse

// Zeit für die Anzeige: auf 0,01 s gerundet, z. B. "12.34". buf mindestens 8 Zeichen.
void ss_format_time(char *buf, uint32_t t_us);
