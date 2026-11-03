#!/usr/bin/env python3
"""Erzeugt Bitmap-Schriften (4 bit Graustufen, für Antialiasing) als C-Quelltext für die Timer-Firmware.

    python tools/make_font.py

Quelle: firmware/timer/fonts/BarlowSemiCondensed-SemiBold.ttf (SIL Open Font License, siehe OFL.txt)
Ziel:   firmware/timer/src/ui/fonts/font_*.c und fonts.h
Benötigt: pip install pillow fonttools

Zeichen, die die Schrift nicht enthält (z. B. ▲ ▼ ◄ ●), werden übersprungen;
die Grafikbibliothek zeichnet diese Symbole selbst.
"""

import pathlib

from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
TTF = ROOT / "firmware/timer/fonts/BarlowSemiCondensed-SemiBold.ttf"
OUT = ROOT / "firmware/timer/src/ui/fonts"

TEXT = "".join(chr(c) for c in range(32, 127)) + "ÄÖÜäöüß°·–…›●▲▼◄Øµ"
DIGITS = " 0123456789.:-–+%s"

# Name, Pixelgröße, Zeichensatz
FONTS = [
    ("f12", 12, TEXT),
    ("f16", 16, TEXT),
    ("f20", 20, TEXT),
    ("f26", 26, TEXT),
    ("f44", 44, TEXT),
    ("f84", 84, DIGITS),
]


def render_glyph(font, ch):
    left, top, right, bottom = font.getbbox(ch, anchor="ls")
    adv = round(font.getlength(ch))
    w, h = max(0, right - left), max(0, bottom - top)
    if w == 0 or h == 0:
        return adv, 0, 0, 0, 0, b""
    img = Image.new("L", (w, h), 0)
    ImageDraw.Draw(img).text((-left, -top), ch, font=font, fill=255, anchor="ls")
    px = img.load()
    data = bytearray()
    for y in range(h):
        row = [px[x, y] >> 4 for x in range(w)]
        if len(row) % 2:
            row.append(0)
        data.extend((row[i] << 4) | row[i + 1] for i in range(0, len(row), 2))
    return adv, w, h, left, top, bytes(data)


CMAP = TTFont(str(TTF)).getBestCmap()


def make(name, size, chars):
    font = ImageFont.truetype(str(TTF), size)
    ascent, descent = font.getmetrics()
    glyphs, bitmap = [], bytearray()
    for ch in sorted(set(chars), key=ord):
        if ord(ch) not in CMAP:
            continue
        adv, w, h, x0, y0, data = render_glyph(font, ch)
        glyphs.append((ord(ch), w, h, x0, y0, adv, len(bitmap)))
        bitmap.extend(data)
    lines = [
        "// Automatisch erzeugt von tools/make_font.py, nicht von Hand ändern.",
        "// Schrift: Barlow Semi Condensed SemiBold, SIL Open Font License 1.1 (fonts/OFL.txt)",
        '#include "fonts.h"',
        "",
        f"static const uint8_t s_bitmap[{max(1, len(bitmap))}] = {{",
    ]
    for i in range(0, len(bitmap), 24):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in bitmap[i:i + 24]) + ",")
    if not bitmap:
        lines.append("    0,")
    lines += ["};", "", f"static const font_glyph_t s_glyphs[{len(glyphs)}] = {{"]
    for cp, w, h, x0, y0, adv, off in glyphs:
        lines.append(f"    {{ 0x{cp:04x}, {w}, {h}, {x0}, {y0}, {adv}, {off} }},")
    lines += [
        "};",
        "",
        f"const font_t font_{name} = {{",
        f"    .size = {size}, .ascent = {ascent}, .descent = {descent},",
        f"    .count = {len(glyphs)}, .glyphs = s_glyphs, .bitmap = s_bitmap,",
        "};",
        "",
    ]
    (OUT / f"font_{name}.c").write_text("\n".join(lines), encoding="utf-8")
    print(f"font_{name}: {len(glyphs)} Zeichen, {len(bitmap)} Byte")


HEADER = """// Automatisch erzeugt von tools/make_font.py, nicht von Hand ändern.
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

"""


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, size, chars in FONTS:
        make(name, size, chars)
    decls = "".join(f"extern const font_t font_{name};\n" for name, _, _ in FONTS)
    (OUT / "fonts.h").write_text(HEADER + decls, encoding="utf-8")


if __name__ == "__main__":
    main()
