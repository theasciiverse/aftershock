/* Aftershock: the chafa backend (--video=chafa, Pictures 2 to 4).
 *
 * chafa is LGPL-3.0-or-later, so a build with it is distributed under
 * GPL-3.0-or-later; `make CHAFA=0` leaves it out.  Written for chafa 1.18
 * (it has no canvas alignment setters; those belong to placements).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_pictures.h"
#include "av_term.h"

#include <chafa.h>
#include <string.h>

static ChafaCanvas *canvas;
static ChafaTermInfo *term_info;
static av_config conf;

static void drop_canvas(void)
{
    if (canvas)
        chafa_canvas_unref(canvas);
    canvas = NULL;
}

static int build(const av_config *cfg)
{
    ChafaCanvasConfig *cc;
    ChafaSymbolMap *map;
    ChafaSymbolTags tags;
    ChafaCanvasMode mode;

    conf = *cfg;
    drop_canvas();

    switch (cfg->symbols) {
    case AV_SYM_ASCII:
        tags = CHAFA_SYMBOL_TAG_ASCII | CHAFA_SYMBOL_TAG_SPACE;
        break;
    case AV_SYM_BLOCK:
        tags = CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_SPACE;
        break;
    default:
        tags = CHAFA_SYMBOL_TAG_BLOCK | CHAFA_SYMBOL_TAG_SPACE |
               CHAFA_SYMBOL_TAG_HALF | CHAFA_SYMBOL_TAG_QUAD |
               CHAFA_SYMBOL_TAG_SEXTANT | CHAFA_SYMBOL_TAG_OCTANT;
        break;
    }
    switch (cfg->color) {
    case AV_COLOR_MONO: mode = CHAFA_CANVAS_MODE_FGBG; break;
    case AV_COLOR_16:   mode = CHAFA_CANVAS_MODE_INDEXED_16; break;
    case AV_COLOR_256:  mode = CHAFA_CANVAS_MODE_INDEXED_240; break;
    default:            mode = CHAFA_CANVAS_MODE_TRUECOLOR; break;
    }

    map = chafa_symbol_map_new();
    chafa_symbol_map_add_by_tags(map, tags);
    cc = chafa_canvas_config_new();
    chafa_canvas_config_set_geometry(cc, cfg->cols > 0 ? cfg->cols : 1,
                                     cfg->rows > 0 ? cfg->rows : 1);
    /* The real cell shape, so a 4:3 picture is not squashed by 1:2
     * cells. */
    if (cfg->cell_w > 0 && cfg->cell_h > 0)
        chafa_canvas_config_set_cell_geometry(cc, cfg->cell_w, cfg->cell_h);
    chafa_canvas_config_set_pixel_mode(cc, CHAFA_PIXEL_MODE_SYMBOLS);
    chafa_canvas_config_set_symbol_map(cc, map);
    chafa_canvas_config_set_canvas_mode(cc, mode);
    /* Escape volume is what saturates the terminal. */
    chafa_canvas_config_set_optimizations(cc, CHAFA_OPTIMIZATION_ALL);
    chafa_canvas_config_set_work_factor(cc, 0.15f);
    /* Error diffusion shimmers in motion. */
    chafa_canvas_config_set_dither_mode(cc, mode == CHAFA_CANVAS_MODE_TRUECOLOR
                                        ? CHAFA_DITHER_MODE_NONE
                                        : CHAFA_DITHER_MODE_ORDERED);
    canvas = chafa_canvas_new(cc);
    chafa_canvas_config_unref(cc);
    chafa_symbol_map_unref(map);
    return canvas ? 0 : -1;
}

static int chafa_init(const av_config *cfg)
{
    if (!term_info) {
        gchar **env = g_get_environ();

        term_info = chafa_term_db_detect(chafa_term_db_get_default(), env);
        g_strfreev(env);
    }
    return build(cfg);
}

static void chafa_shutdown(void)
{
    drop_canvas();
    if (term_info)
        chafa_term_info_unref(term_info);
    term_info = NULL;
}

static int chafa_resize(const av_config *cfg)
{
    return build(cfg);
}

static void chafa_frame(const uint8_t *rgb, int w, int h, int stride)
{
    GString *s;
    size_t n;

    if (!canvas)
        return;
    chafa_canvas_draw_all_pixels(canvas, CHAFA_PIXEL_RGB8, rgb, w, h, stride);
    s = chafa_canvas_print(canvas, term_info);
    if (!s)
        return;
    n = s->len;
    /* A newline on the bottom row scrolls the picture up a line a
     * frame. */
    while (n > 0 && (s->str[n - 1] == '\n' || s->str[n - 1] == '\r'))
        n--;
    av_term_write_at(s->str, n, conf.origin_row, conf.origin_col);
    g_string_free(s, TRUE);
}

const av_backend av_backend_chafa = {
    "chafa", chafa_init, chafa_shutdown, chafa_resize, chafa_frame
};
