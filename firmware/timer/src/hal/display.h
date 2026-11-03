// ST7789-Display (2,0", 240 × 320, quer betrieben als 320 × 240) über SPI mit DMA.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"

void display_init(void);
void display_draw(const gfx_t *g);          // rastert in Streifen und überträgt per DMA
void display_backlight(uint8_t percent);
void display_power(bool on);                // aus: Panel schlafen legen, Beleuchtung aus
