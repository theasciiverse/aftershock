/* Aftershock: the table of presentation backends and the Picture numbers.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_pictures.h"

#include <stddef.h>
#include <string.h>

/* The order --video and the help text list them in. */
static const av_backend *const backends[] = {
    &av_backend_half,
    &av_backend_sextant,
    &av_backend_braille,
#ifdef AV_HAVE_CHAFA
    &av_backend_chafa,
#endif
#ifdef AV_HAVE_CACA
    &av_backend_caca,
#endif
};
#define NBACKENDS (sizeof(backends) / sizeof(backends[0]))

/* Picture number to backend name and glyph set (-1: any). */
static const struct {
    const char *backend;
    int symbols;
} pictures[AV_PICTURE_COUNT] = {
    { "half", -1 },
    { "sextant", -1 },
    { "chafa", AV_SYM_FINE },
    { "chafa", AV_SYM_BLOCK },
    { "chafa", AV_SYM_ASCII },
    { "caca", -1 },
    { "braille", -1 },
};

void av_config_init(av_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->origin_col = 1;
    cfg->origin_row = 1;
}

const av_backend *av_backend_find(const char *name)
{
    size_t i;

    for (i = 0; i < NBACKENDS; i++)
        if (strcmp(backends[i]->name, name) == 0)
            return backends[i];
    return NULL;
}

const char *av_backend_names(void)
{
    static char names[128];
    size_t i;

    if (!names[0]) {
        for (i = 0; i < NBACKENDS; i++) {
            if (i)
                strcat(names, ",");
            strcat(names, backends[i]->name);
        }
    }
    return names;
}

int av_picture_of(const av_backend *be, int symbols)
{
    int n, first = -1;

    if (!be)
        return -1;
    for (n = 0; n < AV_PICTURE_COUNT; n++) {
        if (strcmp(pictures[n].backend, be->name) != 0)
            continue;
        if (pictures[n].symbols == symbols)
            return n;
        if (first < 0)
            first = n;
    }
    return first;
}

const av_backend *av_picture_backend(int n, int *symbols)
{
    const av_backend *be;

    if (n < 0 || n >= AV_PICTURE_COUNT)
        return NULL;
    be = av_backend_find(pictures[n].backend);
    if (be && symbols && pictures[n].symbols >= 0)
        *symbols = pictures[n].symbols;
    return be;
}

unsigned av_pictures_mask(void)
{
    unsigned mask = 0;
    int n;

    for (n = 0; n < AV_PICTURE_COUNT; n++)
        if (av_backend_find(pictures[n].backend))
            mask |= 1u << n;
    return mask;
}
