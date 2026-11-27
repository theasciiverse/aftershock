/* Aftershock: presentation backends and the Picture setting.
 *
 * A backend turns an RGB888 frame into terminal output.  The Picture
 * setting (av_picture in config.cfg, Options, Video) names a backend plus
 * a glyph set by number.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_PICTURES_H
#define AV_PICTURES_H

#include <stdint.h>

enum { AV_SYM_ASCII, AV_SYM_BLOCK, AV_SYM_FINE };
enum { AV_COLOR_MONO, AV_COLOR_16, AV_COLOR_256, AV_COLOR_TRUE };

typedef struct {
    int symbols;            /* AV_SYM_* */
    int color;              /* AV_COLOR_* */
    int cols, rows;         /* canvas, in cells */
    int cell_w, cell_h;     /* one cell, in pixels */
    int origin_col;         /* canvas top left, 1-based terminal position */
    int origin_row;
} av_config;

typedef struct {
    const char *name;
    int (*init)(const av_config *cfg);
    void (*shutdown)(void);
    int (*resize)(const av_config *cfg);
    void (*frame)(const uint8_t *rgb, int w, int h, int stride);
} av_backend;

/* Zeroed with the origin at 1,1.  Every config starts here: an unset
 * origin once sent a cursor move to row 1600019804. */
void av_config_init(av_config *cfg);

/* NULL when no backend of that name is compiled in. */
const av_backend *av_backend_find(const char *name);
/* "half,sextant,..." for messages and the help text. */
const char *av_backend_names(void);

/* Picture numbers.  They are the menu order and are saved, so they are
 * only ever appended to: 0 half, 1 sextant, 2 chafa fine, 3 chafa block,
 * 4 chafa ASCII, 5 caca, 6 Braille. */
#define AV_PICTURE_COUNT 7
/* -1 when the backend has no number. */
int av_picture_of(const av_backend *be, int symbols);
/* The backend for picture n, or NULL when it is not compiled in.  *symbols
 * is set when the number fixes the glyph set, else left alone. */
const av_backend *av_picture_backend(int n, int *symbols);
/* Bit n set when picture n can be shown. */
unsigned av_pictures_mask(void);

/* Our half-block family (half, sextant, Braille) lays out its own text and
 * positions every cell itself.  In av_half.c. */
extern const av_backend av_backend_half, av_backend_sextant,
                        av_backend_braille;
int av_half_family(const av_backend *be);
void av_half_resolution(int cols, int rows, int *w, int *h);

#ifdef AV_HAVE_CHAFA
extern const av_backend av_backend_chafa;
#endif
#ifdef AV_HAVE_CACA
extern const av_backend av_backend_caca;
#endif

#endif
