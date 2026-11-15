/*
 * Aftershock: family-friendly effects and the game's own words.
 *
 * Copyright (C) 2026 the Aftershock authors.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * Two things live here, both applied where the engine meets the data, so the
 * game code (QuakeC) and the maps need no change:
 *
 *  - av_gore 0 (the default): blood particles become sparks and dust, gib
 *    and head models load as stone debris, blood trails become smoke, and
 *    the squelching gib sounds become thuds.  av_gore 1 brings back the
 *    original effects (from the next map on, since models stay cached).
 *
 *  - The text table, av_text.txt in the game folder: every line the player
 *    reads from the game (console prints, centre messages, level names,
 *    endings) passes through it, so names and phrases can be replaced.
 *    Lines are "find<TAB>replace"; lines starting with # are comments.
 *    Lines that start with "gore<TAB>" apply only while av_gore is 0.
 */

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compat/strl.h"
#include "console.h"
#include "cvar.h"
#include "quakedef.h"
#include "av_family.h"

cvar_t av_gore = { "av_gore", "0", true };

static const char *const gore_files[][2] = {
    { "progs/gib1.mdl",      "progs/av_rubble1.mdl" },
    { "progs/gib2.mdl",      "progs/av_rubble2.mdl" },
    { "progs/gib3.mdl",      "progs/av_rubble3.mdl" },
    { "progs/zom_gib.mdl",   "progs/av_rubble2.mdl" },
    { "progs/h_player.mdl",  "progs/av_pile.mdl" },
    { "progs/h_dog.mdl",     "progs/av_pile.mdl" },
    { "progs/h_mega.mdl",    "progs/av_pile.mdl" },
    { "progs/h_guard.mdl",   "progs/av_pile.mdl" },
    { "progs/h_knight.mdl",  "progs/av_pile.mdl" },
    { "progs/h_hellkn.mdl",  "progs/av_pile.mdl" },
    { "progs/h_wizard.mdl",  "progs/av_pile.mdl" },
    { "progs/h_ogre.mdl",    "progs/av_pile.mdl" },
    { "progs/h_demon.mdl",   "progs/av_pile.mdl" },
    { "progs/h_shams.mdl",   "progs/av_pile.mdl" },
    { "progs/h_shal.mdl",    "progs/av_pile.mdl" },
    { "progs/h_zombie.mdl",  "progs/av_pile.mdl" },
    { "sound/player/udeath.wav",   "sound/player/land2.wav" },
    { "sound/player/gib.wav",      "sound/player/land2.wav" },
    { "sound/player/tornoff2.wav", "sound/player/land.wav" },
    { "sound/zombie/z_gib.wav",    "sound/weapons/tink1.wav" },
};

void
AV_Family_Init(void)
{
    Cvar_RegisterVariable(&av_gore);
}

const char *
AV_GoreFile(const char *path)
{
    size_t i;

    if (av_gore.value)
        return path;
    for (i = 0; i < sizeof(gore_files) / sizeof(gore_files[0]); i++)
        if (!strcmp(path, gore_files[i][0]))
            return gore_files[i][1];
    return path;
}

/*
 * The game paints blood with palette colour 73 (a red ramp, 72..79 after the
 * renderer's random offset).  Without gore it is sparks: the yellow and
 * orange fire ramp, which also reads as hits landing.
 */
int
AV_ParticleColor(int color)
{
    if (av_gore.value)
        return color;
    if (color >= 64 && color < 80)
        return 224 + (color & 7) / 2;
    return color;
}

/* Trail types 2 (blood) and 4 (slight blood) become smoke (1). */
int
AV_TrailType(int type)
{
    if (av_gore.value)
        return type;
    if (type == 2 || type == 4)
        return 1;
    return type;
}

/* ---- the text table ---------------------------------------------------- */

#define AV_MAX_RULES 512

struct rule {
    const char *find, *repl;
    size_t flen;
    int gore_only;
};

static struct rule rules[AV_MAX_RULES];
static int num_rules;
static int text_loaded;
static char *text_buf;

static void
AV_LoadText(void)
{
    byte stackbuf[1];
    unsigned long len = 0;
    char *p, *end, *line;
    void *data;

    text_loaded = 1;
    /* A zero-size stack buffer makes the loader take hunk temp memory,
     * which the next load reuses, so copy the text out at once. */
    data = COM_LoadStackFile("av_text.txt", stackbuf, 0, &len);
    if (!data || !len)
        return;
    text_buf = malloc(len + 1);
    if (!text_buf)
        return;
    memcpy(text_buf, data, len);
    text_buf[len] = 0;

    p = text_buf;
    end = text_buf + len;
    while (p < end && num_rules < AV_MAX_RULES) {
        char *nl = strchr(p, '\n'), *tab;
        int gore_only = 0;

        if (!nl)
            nl = end;
        *nl = 0;
        if (nl > p && nl[-1] == '\r')
            nl[-1] = 0;
        line = p;
        p = nl + 1;
        if (!line[0] || line[0] == '#')
            continue;
        if (!strncmp(line, "gore\t", 5)) {
            gore_only = 1;
            line += 5;
        }
        tab = strchr(line, '\t');
        if (!tab || tab == line)
            continue;
        *tab = 0;
        rules[num_rules].find = line;
        rules[num_rules].repl = tab + 1;
        rules[num_rules].flen = strlen(line);
        rules[num_rules].gore_only = gore_only;
        num_rules++;
    }
    Con_DPrintf("av_text.txt: %d replacements\n", num_rules);
}

/*
 * Returns the text with every rule applied, in one of two static buffers
 * used in turn, so a caller may hold one result while asking for another.
 */
const char *
AV_Text(const char *in)
{
    static char res[2][4096], tmp[4096];
    static int which;
    char *a;
    int i;

    if (!text_loaded)
        AV_LoadText();
    if (!num_rules || !in)
        return in;

    which ^= 1;
    a = res[which];
    strlcpy(a, in, sizeof(res[0]));

    for (i = 0; i < num_rules; i++) {
        const struct rule *r = &rules[i];
        const char *s, *hit;
        size_t o = 0, rlen;

        if (r->gore_only && av_gore.value)
            continue;
        if (!strstr(a, r->find))
            continue;
        rlen = strlen(r->repl);
        s = a;
        while ((hit = strstr(s, r->find)) != NULL) {
            size_t pre = (size_t)(hit - s);

            if (o + pre + rlen + 1 >= sizeof(tmp))
                break;
            memcpy(tmp + o, s, pre);
            o += pre;
            memcpy(tmp + o, r->repl, rlen);
            o += rlen;
            s = hit + r->flen;
        }
        strlcpy(tmp + o, s, sizeof(tmp) - o);
        strlcpy(a, tmp, sizeof(res[0]));
    }
    return a;
}
