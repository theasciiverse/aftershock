/* Aftershock: the half-block presentation backend.
 *
 * Each character cell shows two square pixels: the upper half block (U+2580)
 * in the upper pixel's colour, on a background of the lower pixel's colour,
 * both in 24-bit colour.  A cell whose two pixels match is sent as a space
 * on that background, which is shorter.
 *
 * The frame is averaged down to the cell grid (each output pixel is the mean
 * of the source pixels it covers, which also smooths the edges), fitted at
 * the source's own aspect and centred.  The engine is asked for a resolution
 * whose aspect matches the grid exactly (see av_half_resolution), so at the
 * start nothing is wasted; after a resize the picture keeps its shape.
 *
 * Only cells that changed since the last frame are sent, with colours and
 * the cursor position emitted only when they differ from what the terminal
 * already has, and the whole frame wrapped in synchronised output
 * (CSI ? 2026) so the terminal never shows half a frame.  This is the way
 * our doom-ascii's `-chars half` works, and the reason it is cheap: a still
 * scene costs nothing, and a moving one at 100x30 is a few tens of KB.
 *
 * No library is involved; chafa stays available as --video=chafa.
 */
#include "oq_present.h"
#include "oq_term.h"
#include "av_overlay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t top, bot;      /* 0xRRGGBB */
    uint32_t ch;            /* 0 = picture cell, else an overlay character */
} cell;

static oq_present_config conf;
static cell *prev, *cur;
static int grid_w, grid_h;      /* cells */
static int force_full;
static char *out;
static size_t out_cap, out_len;

static void out_need(size_t n)
{
    if (out_len + n <= out_cap)
        return;
    out_cap = (out_len + n) * 2;
    out = realloc(out, out_cap);
    if (!out) {
        out_cap = out_len = 0;
    }
}

static void out_str(const char *s, size_t n)
{
    out_need(n);
    if (!out)
        return;
    memcpy(out + out_len, s, n);
    out_len += n;
}

static void out_utf8(uint32_t c)
{
    char b[4];
    size_t n;

    if (c < 0x80) {
        b[0] = (char)c; n = 1;
    } else if (c < 0x800) {
        b[0] = (char)(0xC0 | (c >> 6));
        b[1] = (char)(0x80 | (c & 0x3F)); n = 2;
    } else if (c < 0x10000) {
        b[0] = (char)(0xE0 | (c >> 12));
        b[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        b[2] = (char)(0x80 | (c & 0x3F)); n = 3;
    } else {
        b[0] = (char)(0xF0 | (c >> 18));
        b[1] = (char)(0x80 | ((c >> 12) & 0x3F));
        b[2] = (char)(0x80 | ((c >> 6) & 0x3F));
        b[3] = (char)(0x80 | (c & 0x3F)); n = 4;
    }
    out_str(b, n);
}

/* Fast decimal for 0..255 colour components and small coordinates. */
static void out_num(int v)
{
    char b[12];
    int n = 0, i;

    if (v == 0) {
        out_str("0", 1);
        return;
    }
    while (v > 0 && n < 11) {
        b[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    out_need((size_t)n);
    if (!out)
        return;
    for (i = n - 1; i >= 0; i--)
        out[out_len++] = b[i];
}

static void out_rgb(const char *lead, uint32_t c)
{
    out_str(lead, strlen(lead));
    out_num((int)(c >> 16) & 0xff);
    out_str(";", 1);
    out_num((int)(c >> 8) & 0xff);
    out_str(";", 1);
    out_num((int)c & 0xff);
    out_str("m", 1);
}

static int alloc_grid(int w, int h)
{
    size_t n = (size_t)w * h;

    free(prev);
    free(cur);
    prev = calloc(n ? n : 1, sizeof(cell));
    cur = calloc(n ? n : 1, sizeof(cell));
    if (!prev || !cur)
        return -1;
    grid_w = w;
    grid_h = h;
    force_full = 1;
    return 0;
}

static int style;               /* 0 half, 1 sextants, 2 Braille: below */

static int half_init(const oq_present_config *cfg)
{
    style = 0;
    conf = *cfg;
    return alloc_grid(cfg->cols, cfg->rows);
}

static void half_shutdown(void)
{
    free(prev);
    free(cur);
    free(out);
    prev = cur = NULL;
    out = NULL;
    out_cap = out_len = 0;
}

static int half_resize(const oq_present_config *cfg)
{
    conf = *cfg;
    return alloc_grid(cfg->cols, cfg->rows);
}

/* Average the source rectangle [x0,x1) x [y0,y1) (in 16.16 fixed point
 * source coordinates, so every source pixel is weighted by how much of it
 * the output pixel covers along each axis... approximated here by whole
 * pixels, which is plenty when the source is a whole multiple of the grid). */
static uint32_t box(const uint8_t *src, int stride, int x0, int x1,
                    int y0, int y1)
{
    unsigned r = 0, g = 0, b = 0, n = 0;
    int x, y;

    if (x1 <= x0) x1 = x0 + 1;
    if (y1 <= y0) y1 = y0 + 1;
    for (y = y0; y < y1; y++) {
        const uint8_t *p = src + (size_t)y * stride + (size_t)x0 * 3;

        for (x = x0; x < x1; x++, p += 3) {
            r += p[0];
            g += p[1];
            b += p[2];
            n++;
        }
    }
    r /= n; g /= n; b /= n;
    return (r << 16) | (g << 8) | b;
}

/* --video=sextant and --video=braille: each cell is a small grid of
 * sub-pixels in two colours.  Sextants (2x3, U+1FB00 on, Symbols for Legacy
 * Computing) fill the cell; Braille (2x4 dots, U+2800 on) draws dots, which
 * come out square on a cell twice as tall as wide, with the second colour
 * filling between them.  Sharper edges than half blocks, at the cost of two
 * colours per cell.  `style`: 0 half blocks, 1 sextants, 2 Braille. */
static int style;

static unsigned lum(uint32_t c)
{
    return ((c >> 16) & 255) * 3 + ((c >> 8) & 255) * 6 + (c & 255);
}

static uint32_t mean(const uint32_t *c, int n, int mask, int want)
{
    unsigned r = 0, g = 0, b = 0, k = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (((mask >> i) & 1) != want)
            continue;
        r += (c[i] >> 16) & 255;
        g += (c[i] >> 8) & 255;
        b += c[i] & 255;
        k++;
    }
    if (!k)
        return 0;
    return ((r / k) << 16) | ((g / k) << 8) | (b / k);
}

/* Braille's dot bits for the 2x4 grid read row by row. */
static const unsigned char braille_bit[8] = {
    0x01, 0x08, 0x02, 0x10, 0x04, 0x20, 0x40, 0x80
};

static void split_cell(cell *k, const uint8_t *src, int stride, int w,
                       int h, int pw, int ph, int gx, int gy)
{
    /* The cell spans [gx, gx+1) by [gy, gy+2) in half-block pixels; ny is
     * the sub-pixel rows (3 or 4), 2 columns always.  Coordinates below are
     * in halves (x) and ny-ths (y) of source pixels. */
    int ny = style == 2 ? 4 : 3, n = 2 * ny;
    long x0 = (long)gx * w * 2 / pw, x2 = (long)(gx + 1) * w * 2 / pw;
    long ytop = (long)gy * h * ny / ph, ybot = (long)(gy + 2) * h * ny / ph;
    uint32_t sub[8];
    unsigned l[8], avg = 0;
    int i, mask = 0, full = (1 << n) - 1;

    if (ybot > (long)h * ny)
        ybot = (long)h * ny;
    for (i = 0; i < n; i++) {
        int sx = i & 1, sy = i >> 1;
        long ax = x0 + (x2 - x0) * sx / 2, bx = x0 + (x2 - x0) * (sx + 1) / 2;
        long ay = ytop + (ybot - ytop) * sy / ny;
        long by = ytop + (ybot - ytop) * (sy + 1) / ny;

        sub[i] = box(src, stride, (int)(ax / 2), (int)((bx + 1) / 2),
                     (int)(ay / ny), (int)((by + ny - 1) / ny));
        l[i] = lum(sub[i]);
        avg += l[i];
    }
    avg /= n;
    for (i = 0; i < n; i++)
        if (l[i] > avg)
            mask |= 1 << i;
    if (mask == 0 || mask == full) {
        k->top = k->bot = mean(sub, n, 0, 0);   /* one colour: all of them */
        return;
    }
    k->top = mean(sub, n, mask, 1);
    k->bot = mean(sub, n, mask, 0);
    if (style == 2) {
        unsigned dots = 0;

        for (i = 0; i < 8; i++)
            if (mask & (1 << i))
                dots |= braille_bit[i];
        k->ch = 0x2800 + dots;
    } else if (mask == 21) {
        k->ch = 0x258C;         /* left half block */
    } else if (mask == 42) {
        k->ch = 0x2590;         /* right half block */
    } else {
        k->ch = 0x1FB00 + mask - 1 - (mask > 21) - (mask > 42);
    }
}

static void half_frame(const uint8_t *src, int w, int h, int stride)
{
    int pw, ph, ox, oy;         /* picture size and offset, in grid pixels */
    int gw = grid_w, gh = (grid_h - av_overlay_reserved_rows()) * 2;
    int x, y, r, c;
    uint32_t fg = 0xffffffff, bg = 0xffffffff;
    int cur_r = -1, cur_c = -1;
    int changed = 0;

    if (!cur || gw <= 0 || gh <= 0 || w <= 0 || h <= 0)
        return;

    /* Fit at the source aspect, assuming square terminal half-cells. */
    pw = gw;
    ph = (int)((long)gw * h / w);
    if (ph > gh) {
        ph = gh;
        pw = (int)((long)gh * w / h);
    }
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;
    ox = (gw - pw) / 2;
    oy = ((gh - ph) / 2) & ~1;  /* keep the picture on a cell boundary */

    for (r = 0; r < grid_h; r++) {
        for (c = 0; c < grid_w; c++) {
            cell *k = &cur[r * grid_w + c];
            int gx = c - ox, gy = r * 2 - oy;

            k->ch = 0;
            if (gx < 0 || gx >= pw || gy < 0 || gy + 1 >= ph + 1 ||
                gy >= ph) {
                k->top = k->bot = 0;
                continue;
            }
            if (style) {
                split_cell(k, src, stride, w, h, pw, ph, gx, gy);
                continue;
            }
            {
                uint32_t px[2];
                int i;

                for (i = 0; i < 2; i++) {
                    int yy = gy + i;

                    if (yy >= ph) {
                        px[i] = 0;
                        continue;
                    }
                    x = (int)((long)gx * w / pw);
                    y = (int)((long)yy * h / ph);
                    px[i] = box(src, stride, x,
                                (int)((long)(gx + 1) * w / pw), y,
                                (int)((long)(yy + 1) * h / ph));
                }
                k->top = px[0];
                k->bot = px[1];
            }
        }
    }

    av_overlay_apply_cells(grid_w, grid_h, &cur[0].top, &cur[0].bot,
                           &cur[0].ch, sizeof(cell));

    out_len = 0;
    out_str("\033[?2026h", 8);
    if (force_full)
        out_str("\033[0m", 4);

    for (r = 0; r < grid_h; r++) {
        for (c = 0; c < grid_w; c++) {
            cell *k = &cur[r * grid_w + c];
            cell *o = &prev[r * grid_w + c];

            if (!force_full && k->top == o->top && k->bot == o->bot &&
                k->ch == o->ch)
                continue;
            changed++;
            if (cur_r != r || cur_c != c) {
                out_str("\033[", 2);
                out_num(conf.origin_row + r);
                out_str(";", 1);
                out_num(conf.origin_col + c);
                out_str("H", 1);
            }
            if (k->ch) {
                /* Text: top is the ink, bot the paper. */
                if (fg != k->top) { out_rgb("\033[38;2;", k->top); fg = k->top; }
                if (bg != k->bot) { out_rgb("\033[48;2;", k->bot); bg = k->bot; }
                out_utf8(k->ch);
            } else if (k->top == k->bot) {
                if (bg != k->bot) { out_rgb("\033[48;2;", k->bot); bg = k->bot; }
                out_str(" ", 1);
            } else {
                if (fg != k->top) { out_rgb("\033[38;2;", k->top); fg = k->top; }
                if (bg != k->bot) { out_rgb("\033[48;2;", k->bot); bg = k->bot; }
                out_str("\xe2\x96\x80", 3);
            }
            cur_r = r;
            cur_c = c + 1;
        }
    }
    out_str("\033[?2026l", 8);

    /* Development aid: AV_DUMP_FRAME=path writes the 150th engine frame as
     * a PPM, to compare picture modes away from a terminal. */
    {
        static unsigned n;
        const char *dump = getenv("AV_DUMP_FRAME");

        if (dump && ++n == 150) {
            FILE *f = fopen(dump, "wb");
            if (f) {
                int y;
                fprintf(f, "P6\n%d %d\n255\n", w, h);
                for (y = 0; y < h; y++)
                    fwrite(src + (size_t)y * stride, 3, (size_t)w, f);
                fclose(f);
            }
        }
    }
    if (out && (changed || force_full))
        oq_term_write_raw(out, out_len);
    force_full = 0;

    {
        cell *t = prev;
        prev = cur;
        cur = t;
    }
}

const oq_present_backend av_backend_half = {
    "half", half_init, half_shutdown, half_resize, half_frame
};

static int sextant_init(const oq_present_config *cfg)
{
    int r = half_init(cfg);

    style = 1;
    return r;
}

const oq_present_backend av_backend_sextant = {
    "sextant", sextant_init, half_shutdown, half_resize, half_frame
};

static int braille_init(const oq_present_config *cfg)
{
    int r = half_init(cfg);

    style = 2;
    return r;
}

const oq_present_backend av_backend_braille = {
    "braille", braille_init, half_shutdown, half_resize, half_frame
};

int av_half_family(const oq_present_backend *be)
{
    return be == &av_backend_half || be == &av_backend_sextant ||
           be == &av_backend_braille;
}

/* The engine resolution that fills a grid of cols x rows cells exactly:
 * the half-block grid's own aspect (cols by rows*2 square pixels), scaled by
 * a whole number so each output pixel averages a k x k block, and never
 * below the 200 lines Quake's renderer is designed for. */
void av_half_resolution(int cols, int rows, int *w, int *h)
{
    int gw = cols > 0 ? cols : 80, gh = (rows > 0 ? rows : 24) * 2;
    int k = 1;

    while (gh * k < 200 || gw * k < 320)
        k++;
    while (gw * k > 1920 || gh * k > 1200)
        k--;
    if (k < 1)
        k = 1;
    *w = gw * k;
    *h = gh * k;
    if (*w < 320) *w = 320;
    if (*h < 200) *h = 200;
}
