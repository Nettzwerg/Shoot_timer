// Piezo-Ansteuerung über DRV8833 (Vollbrücke) an MT3608-Boost
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BEEP_VOL_FULL = 0,  // Vollbrücke, ca. 20 Vss
    BEEP_VOL_HALF = 1,  // Halbbrücke, ca. 10 Vss (-6 dB)
    BEEP_VOL_LOW = 2,   // Halbbrücke, Tastgrad 10 % (ca. -10 dB)
} beep_volume_t;

void beeper_init(void);
void beeper_power(bool on);     // Boost-Wandler ein/aus
bool beeper_powered(void);
void beeper_start(uint32_t freq_hz, beep_volume_t vol);
void beeper_stop(void);
void beeper_beep(uint32_t freq_hz, uint32_t ms, beep_volume_t vol);  // blockierend
