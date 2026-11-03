#include "gfx.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum { OP_RECT = 0, OP_CIRCLE, OP_DIM, OP_TEXT };

// Symbole, die die Schrift nicht enthält, werden als Formen gezeichnet
enum { SYM_UP = 0x25b2, SYM_DOWN = 0x25bc, SYM_LEFT = 0x25c4, SYM_DOT = 0x25cf };

static gfx_op_t *new_op(gfx_t *g, uint8_t type)
{
    if (g->n >= GFX_MAX_OPS) {
        g->overflow = true;
        return NULL;
    }
    gfx_op_t *op = &g->ops[g->n++];
    memset(op, 0, sizeof(*op));
    op->type = type;
    return op;
}

void gfx_begin(gfx_t *g, gfx_color_t bg)
{
    g->bg = bg;
    g->n = 0;
    g->overflow = false;
}

void gfx_rect(gfx_t *g, int x, int y, int w, int h, gfx_color_t c)
{
    gfx_op_t *op = new_op(g, OP_RECT);
    if (op) {
        op->x = x, op->y = y, op->w = w, op->h = h, op->color = c;
    }
}

void gfx_circle(gfx_t *g, int cx, int cy, int r, gfx_color_t c)
{
    gfx_op_t *op = new_op(g, OP_CIRCLE);
    if (op) {
        op->x = cx, op->y = cy, op->w = r, op->color = c;
    }
}

void gfx_dim(gfx_t *g, int x, int y, int w, int h, uint8_t alpha)
{
    gfx_op_t *op = new_op(g, OP_DIM);
    if (op) {
        op->x = x, op->y = y, op->w = w, op->h = h, op->alpha = alpha;
    }
}

// ---------- Text ----------

static uint32_t utf8_next(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    uint32_t cp;
    int n;
    if (p[0] < 0x80) {
        cp = p[0], n = 1;
    } else if ((p[0] & 0xe0) == 0xc0 && p[1]) {
        cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f), n = 2;
    } else if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
        cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f), n = 3;
    } else {
        cp = '?', n = 1;
    }
    *s += n;
    return cp;
}

static const font_glyph_t *find_glyph(const font_t *f, uint32_t cp)
{
    int lo = 0, hi = f->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) {
            return &f->glyphs[mid];
        }
        if (f->glyphs[mid].cp < cp) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return NULL;
}

static bool is_symbol(uint32_t cp)
{
    return cp == SYM_UP || cp == SYM_DOWN || cp == SYM_LEFT || cp == SYM_DOT;
}

static int symbol_adv(const font_t *f)
{
    return f->size * 3 / 4;
}

static int char_adv(const font_t *f, uint32_t cp)
{
    const font_glyph_t *gl = find_glyph(f, cp);
    if (gl) {
        return gl->adv;
    }
    if (is_symbol(cp)) {
        return symbol_adv(f);
    }
    gl = find_glyph(f, '?');
    return gl ? gl->adv : f->size / 2;
}

int gfx_text_width(const font_t *f, const char *s)
{
    int w = 0;
    while (*s) {
        w += char_adv(f, utf8_next(&s));
    }
    return w;
}

void gfx_text(gfx_t *g, const font_t *f, int x, int baseline, gfx_align_t a, gfx_color_t c, const char *s)
{
    gfx_op_t *op = new_op(g, OP_TEXT);
    if (!op) {
        return;
    }
    strncpy(op->text, s, GFX_TEXT_MAX - 1);
    int w = gfx_text_width(f, op->text);
    op->x = a == GFX_CENTER ? x - w / 2 : a == GFX_RIGHT ? x - w : x;
    op->y = baseline;
    op->w = w;
    op->font = f;
    op->color = c;
}

void gfx_textf(gfx_t *g, const font_t *f, int x, int baseline, gfx_align_t a, gfx_color_t c, const char *fmt, ...)
{
    char buf[GFX_TEXT_MAX];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gfx_text(g, f, x, baseline, a, c, buf);
}

// ---------- Rasterung ----------

typedef struct {
    uint16_t *buf;
    int y0, y1;     // Streifen [y0, y1)
} strip_t;

static inline gfx_color_t blend(gfx_color_t bg, gfx_color_t fg, unsigned a /* 0..15 */)
{
    if (a >= 15) {
        return fg;
    }
    if (a == 0) {
        return bg;
    }
    unsigned r = (((fg >> 11) & 0x1f) * a + ((bg >> 11) & 0x1f) * (15 - a)) / 15;
    unsigned gr = (((fg >> 5) & 0x3f) * a + ((bg >> 5) & 0x3f) * (15 - a)) / 15;
    unsigned b = ((fg & 0x1f) * a + (bg & 0x1f) * (15 - a)) / 15;
    return (gfx_color_t)((r << 11) | (gr << 5) | b);
}

static void fill_span(const strip_t *st, int y, int x0, int x1, gfx_color_t c)
{
    if (y < st->y0 || y >= st->y1) {
        return;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 > GFX_W) {
        x1 = GFX_W;
    }
    uint16_t *row = st->buf + (y - st->y0) * GFX_W;
    for (int x = x0; x < x1; x++) {
        row[x] = c;
    }
}

static void draw_rect(const strip_t *st, const gfx_op_t *op)
{
    int ya = op->y > st->y0 ? op->y : st->y0;
    int yb = op->y + op->h < st->y1 ? op->y + op->h : st->y1;
    for (int y = ya; y < yb; y++) {
        fill_span(st, y, op->x, op->x + op->w, op->color);
    }
}

static void draw_circle(const strip_t *st, const gfx_op_t *op)
{
    int r = op->w;
    for (int dy = -r; dy <= r; dy++) {
        int y = op->y + dy;
        if (y < st->y0 || y >= st->y1) {
            continue;
        }
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r) {
            dx++;
        }
        fill_span(st, y, op->x - dx, op->x + dx + 1, op->color);
    }
}

static void draw_dim(const strip_t *st, const gfx_op_t *op)
{
    int ya = op->y > st->y0 ? op->y : st->y0;
    int yb = op->y + op->h < st->y1 ? op->y + op->h : st->y1;
    unsigned keep = 15 - op->alpha * 15 / 255;
    for (int y = ya; y < yb; y++) {
        uint16_t *row = st->buf + (y - st->y0) * GFX_W;
        for (int x = op->x < 0 ? 0 : op->x; x < op->x + op->w && x < GFX_W; x++) {
            row[x] = blend(0, row[x], keep);
        }
    }
}

// Dreiecke und Punkt für ▲ ▼ ◄ ●, Höhe ca. 0,55 × Schriftgröße, auf der Grundlinie
static void draw_symbol(const strip_t *st, uint32_t cp, int x, int baseline, const font_t *f, gfx_color_t c)
{
    int s = f->size * 11 / 20;
    int left = x + (symbol_adv(f) - s) / 2;
    int top = baseline - s - f->size / 12;
    for (int i = 0; i < s; i++) {
        int y = top + i;
        switch (cp) {
        case SYM_UP: {
            int half = (i + 1) * s / (2 * s);
            fill_span(st, y, left + s / 2 - half, left + s / 2 + half + 1, c);
            break;
        }
        case SYM_DOWN: {
            int half = (s - i) * s / (2 * s);
            fill_span(st, y, left + s / 2 - half, left + s / 2 + half + 1, c);
            break;
        }
        case SYM_LEFT: {
            int dist = i < s / 2 ? i : s - 1 - i;     // Abstand von der Ober-/Unterkante
            int w = dist * 2;
            fill_span(st, y, left + s - w, left + s, c);
            break;
        }
        case SYM_DOT: {
            int r = s / 2, dy = i - r;
            int dx = 0;
            while ((dx + 1) * (dx + 1) + dy * dy <= r * r) {
                dx++;
            }
            fill_span(st, y, left + r - dx, left + r + dx + 1, c);
            break;
        }
        }
    }
}

static void draw_glyph(const strip_t *st, const font_glyph_t *gl, const font_t *f, int x, int baseline, gfx_color_t c)
{
    int gx = x + gl->x0, gy = baseline + gl->y0;
    int stride = (gl->w + 1) / 2;
    const uint8_t *bm = f->bitmap + gl->offset;
    for (int row = 0; row < gl->h; row++) {
        int y = gy + row;
        if (y < st->y0 || y >= st->y1) {
            continue;
        }
        uint16_t *line = st->buf + (y - st->y0) * GFX_W;
        const uint8_t *src = bm + row * stride;
        for (int col = 0; col < gl->w; col++) {
            int px = gx + col;
            if (px < 0 || px >= GFX_W) {
                continue;
            }
            unsigned a = (col & 1) ? (src[col / 2] & 0x0f) : (src[col / 2] >> 4);
            if (a) {
                line[px] = blend(line[px], c, a);
            }
        }
    }
}

static void draw_text(const strip_t *st, const gfx_op_t *op)
{
    const font_t *f = op->font;
    // Grober Test, ob der Text den Streifen überhaupt berührt
    if (op->y + f->descent < st->y0 || op->y - f->size - f->ascent > st->y1) {
        return;
    }
    const char *s = op->text;
    int x = op->x;
    while (*s) {
        uint32_t cp = utf8_next(&s);
        const font_glyph_t *gl = find_glyph(f, cp);
        if (gl) {
            if (gl->w) {
                draw_glyph(st, gl, f, x, op->y, op->color);
            }
        } else if (is_symbol(cp)) {
            draw_symbol(st, cp, x, op->y, f, op->color);
        } else if ((gl = find_glyph(f, '?')) != NULL) {
            draw_glyph(st, gl, f, x, op->y, op->color);
        }
        x += char_adv(f, cp);
    }
}

void gfx_render(const gfx_t *g, int y0, int h, uint16_t *buf, bool swap_bytes)
{
    strip_t st = { .buf = buf, .y0 = y0, .y1 = y0 + h };
    for (int i = 0; i < GFX_W * h; i++) {
        buf[i] = g->bg;
    }
    for (int i = 0; i < g->n; i++) {
        const gfx_op_t *op = &g->ops[i];
        switch (op->type) {
        case OP_RECT:
            draw_rect(&st, op);
            break;
        case OP_CIRCLE:
            draw_circle(&st, op);
            break;
        case OP_DIM:
            draw_dim(&st, op);
            break;
        case OP_TEXT:
            draw_text(&st, op);
            break;
        }
    }
    if (swap_bytes) {
        for (int i = 0; i < GFX_W * h; i++) {
            buf[i] = (uint16_t)((buf[i] << 8) | (buf[i] >> 8));
        }
    }
}
