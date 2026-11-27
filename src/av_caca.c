/* Aftershock: the libcaca backend (--video=caca, Picture 5).
 *
 * Only libcaca's canvas and dither: caca_create_display() is never called,
 * because its drivers take over the tty.  The canvas is exported as UTF-8
 * text with colour escapes and written like any other frame.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_pictures.h"
#include "av_term.h"

#include <caca.h>
#include <stdlib.h>

static caca_canvas_t *cv;
static caca_dither_t *dither;
static int dither_w, dither_h, dither_stride;
static av_config conf;

static void drop_dither(void)
{
    if (dither)
        caca_free_dither(dither);
    dither = NULL;
    dither_w = dither_h = dither_stride = 0;
}

static int caca_init_backend(const av_config *cfg)
{
    conf = *cfg;
    cv = caca_create_canvas(cfg->cols > 0 ? cfg->cols : 1,
                            cfg->rows > 0 ? cfg->rows : 1);
    return cv ? 0 : -1;
}

static void caca_shutdown_backend(void)
{
    drop_dither();
    if (cv)
        caca_free_canvas(cv);
    cv = NULL;
}

static int caca_resize_backend(const av_config *cfg)
{
    conf = *cfg;
    /* Charset and colour may have changed too. */
    drop_dither();
    if (!cv)
        return caca_init_backend(cfg);
    return caca_set_canvas_size(cv, cfg->cols > 0 ? cfg->cols : 1,
                                cfg->rows > 0 ? cfg->rows : 1);
}

static int make_dither(int w, int h, int stride)
{
    const char *charset, *color;

    drop_dither();
    /* libcaca reads the three bytes as a little-endian number, so for
     * R,G,B in memory red is the low byte. */
    dither = caca_create_dither(24, w, h, stride, 0x0000ff, 0x00ff00,
                                0xff0000, 0);
    if (!dither)
        return -1;
    switch (conf.symbols) {
    case AV_SYM_ASCII: charset = "ascii"; break;
    case AV_SYM_BLOCK: charset = "shades"; break;
    default:           charset = "blocks"; break;
    }
    switch (conf.color) {
    case AV_COLOR_MONO: color = "mono"; break;
    case AV_COLOR_16:   color = "16"; break;
    default:            color = "full16"; break;
    }
    caca_set_dither_charset(dither, charset);
    caca_set_dither_color(dither, color);
    caca_set_dither_algorithm(dither, "fstein");
    caca_set_dither_antialias(dither, "prefilter");
    dither_w = w;
    dither_h = h;
    dither_stride = stride;
    return 0;
}

static void caca_frame_backend(const uint8_t *rgb, int w, int h, int stride)
{
    size_t n;
    char *s;

    if (!cv)
        return;
    if (!dither || w != dither_w || h != dither_h || stride != dither_stride)
        if (make_dither(w, h, stride) != 0)
            return;
    caca_dither_bitmap(cv, 0, 0, caca_get_canvas_width(cv),
                       caca_get_canvas_height(cv), dither, rgb);
    s = caca_export_canvas_to_memory(cv, "utf8", &n);
    if (!s)
        return;
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        n--;
    av_term_write_at(s, n, conf.origin_row, conf.origin_col);
    free(s);
}

const av_backend av_backend_caca = {
    "caca", caca_init_backend, caca_shutdown_backend, caca_resize_backend,
    caca_frame_backend
};
