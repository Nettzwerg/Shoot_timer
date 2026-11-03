// Kleine Grafikbibliothek für das 320 × 240-Display.
// Ein Bild wird als Liste von Zeichenbefehlen aufgebaut und anschließend in Zeilenstreifen
// gerastert. So braucht der ESP32 keinen ganzen Framebuffer (150 KB), sondern nur Streifen.
// Plattformunabhängig: wird auch am PC für Screenshots und Tests benutzt.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "fonts/fonts.h"

#define GFX_W           320
#define GFX_H           240
#define GFX_MAX_OPS     200
#define GFX_TEXT_MAX    64      // Bytes UTF-8, Umlaute und Symbole zählen doppelt bzw. dreifach

typedef uint16_t gfx_color_t;   // RGB565
#define GFX_RGB(r, g, b) ((gfx_color_t)((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | ((b) >> 3)))

typedef enum { GFX_LEFT = 0, GFX_CENTER, GFX_RIGHT } gfx_align_t;

typedef struct {
    uint8_t type;
    uint8_t alpha;              // nur für Abdunkeln
    gfx_color_t color;
    int16_t x, y, w, h;         // Text: x = Startpunkt nach Ausrichtung, y = Grundlinie
    const font_t *font;
    char text[GFX_TEXT_MAX];
} gfx_op_t;

typedef struct {
    gfx_color_t bg;
    uint16_t n;
    bool overflow;              // mehr als GFX_MAX_OPS Befehle (Rest wird ignoriert)
    gfx_op_t ops[GFX_MAX_OPS];
} gfx_t;

void gfx_begin(gfx_t *g, gfx_color_t bg);
void gfx_rect(gfx_t *g, int x, int y, int w, int h, gfx_color_t c);
void gfx_circle(gfx_t *g, int cx, int cy, int r, gfx_color_t c);
void gfx_dim(gfx_t *g, int x, int y, int w, int h, uint8_t alpha);  // abdunkeln, alpha 0..255
void gfx_text(gfx_t *g, const font_t *f, int x, int baseline, gfx_align_t a, gfx_color_t c, const char *s);
void gfx_textf(gfx_t *g, const font_t *f, int x, int baseline, gfx_align_t a, gfx_color_t c, const char *fmt, ...)
    __attribute__((format(printf, 7, 8)));
int gfx_text_width(const font_t *f, const char *s);

// Rastert die Zeilen y0 .. y0 + h - 1 nach buf (GFX_W * h Pixel).
// swap_bytes: true für Displays, die das höherwertige Byte zuerst erwarten (ST7789 über SPI).
void gfx_render(const gfx_t *g, int y0, int h, uint16_t *buf, bool swap_bytes);
