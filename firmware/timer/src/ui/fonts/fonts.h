// Automatisch erzeugt von tools/make_font.py, nicht von Hand ändern.
#pragma once

#include <stdint.h>

typedef struct {
    uint16_t cp;        // Unicode-Codepunkt
    uint8_t w, h;       // Größe der Bitmap
    int8_t x0, y0;      // Versatz der Bitmap zur Grundlinie (y0 negativ = oberhalb)
    uint8_t adv;        // Vorschub
    uint32_t offset;    // Start in bitmap[], 4 bit je Pixel, Zeilen auf Byte aufgefüllt
} font_glyph_t;

typedef struct {
    uint8_t size;
    uint8_t ascent;
    uint8_t descent;
    uint16_t count;
    const font_glyph_t *glyphs;     // nach cp sortiert
    const uint8_t *bitmap;
} font_t;

extern const font_t font_f12;
extern const font_t font_f16;
extern const font_t font_f20;
extern const font_t font_f26;
extern const font_t font_f44;
extern const font_t font_f84;
