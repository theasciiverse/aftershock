#include "oq_present.h"
#include <stddef.h>
#include <string.h>

static const oq_present_backend *const backends[] = {
    &av_backend_half,
    &av_backend_sextant,
#ifdef OQ_HAVE_CHAFA
    &oq_backend_chafa,
#endif
#ifdef OQ_HAVE_CACA
    &oq_backend_caca,
#endif
    NULL
};

const oq_present_backend *oq_present_lookup(const char *name)
{
    int i;

    for (i = 0; backends[i]; i++)
        if (!strcmp(backends[i]->name, name))
            return backends[i];
    return NULL;
}

const char *oq_present_available(void)
{
    static char buf[128];
    int i;

    buf[0] = '\0';
    for (i = 0; backends[i]; i++) {
        if (i)
            strncat(buf, ",", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, backends[i]->name, sizeof(buf) - strlen(buf) - 1);
    }
    return buf[0] ? buf : "(none)";
}

/* Aftershock: the Picture setting in Options, Video, by number.  The order
 * is the menu's and is saved in config.cfg as av_picture: never reorder,
 * only append. */
static const struct {
    const oq_present_backend *be;
    oq_symbols symbols;
} pictures[] = {
    { &av_backend_half, OQ_SYMBOLS_FINE },
    { &av_backend_sextant, OQ_SYMBOLS_FINE },
#ifdef OQ_HAVE_CHAFA
    { &oq_backend_chafa, OQ_SYMBOLS_FINE },
    { &oq_backend_chafa, OQ_SYMBOLS_BLOCK },
    { &oq_backend_chafa, OQ_SYMBOLS_ASCII },
#else
    { NULL, OQ_SYMBOLS_FINE }, { NULL, OQ_SYMBOLS_FINE }, { NULL, OQ_SYMBOLS_FINE },
#endif
#ifdef OQ_HAVE_CACA
    { &oq_backend_caca, OQ_SYMBOLS_FINE },
#else
    { NULL, OQ_SYMBOLS_FINE },
#endif
};

#define NPICTURES ((int)(sizeof(pictures) / sizeof(pictures[0])))

const oq_present_backend *oq_present_picture(int p, oq_symbols *symbols)
{
    if (p < 0 || p >= NPICTURES || !pictures[p].be)
        return NULL;
    if (symbols)
        *symbols = pictures[p].symbols;
    return pictures[p].be;
}

int oq_present_picture_of(const oq_present_backend *be, oq_symbols symbols)
{
    int i;

    /* chafa is three pictures, told apart by its symbols */
    for (i = 0; i < NPICTURES; i++)
        if (be && pictures[i].be == be && pictures[i].symbols == symbols)
            return i;
    for (i = 0; i < NPICTURES; i++)
        if (be && pictures[i].be == be)
            return i;
    return -1;
}

unsigned oq_present_pictures(void)
{
    unsigned mask = 0;
    int i;

    for (i = 0; i < NPICTURES; i++)
        if (pictures[i].be)
            mask |= 1u << i;
    return mask;
}
