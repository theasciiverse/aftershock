/*
 * Aftershock: the game's text, handed to the terminal as text.
 * See av_textui.h.
 *
 * Copyright (C) 2026 the Aftershock authors.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <string.h>

#include "client.h"
#include "common.h"
#include "compat/strl.h"
#include "console.h"
#include "cvar.h"
#include "keys.h"
#include "quakedef.h"
#include "sbar.h"
#include "screen.h"
#include "av_textui.h"

cvar_t av_textui = { "av_textui", "1", true };
/* Aftershock: the picture (see aftershock_pictures); saved with the settings. */
cvar_t av_picture = { "av_picture", "0", true };
int aftershock_picture_arg = -1;
unsigned aftershock_pictures = 3;
av_textui_t av_ui;
int av_draw_ctx;

extern qboolean sb_showscores;
extern qboolean scr_drawloading;
extern float scr_con_current;

void
AV_TextUI_Init(void)
{
    Cvar_RegisterVariable(&av_textui);
    Cvar_RegisterVariable(&av_picture);
    /* The frontend's choice from the first frame, before config.cfg runs. */
    if (aftershock_picture_arg >= 0)
        Cvar_SetValue("av_picture", (float)aftershock_picture_arg);
}

void
AV_TextUI_BeginFrame(void)
{
    av_ui.n = 0;
    av_ui.vid_w = vid.width;
    av_ui.vid_h = vid.height;
}

static void
add(int x, int y, int ch, int ctx, int ink)
{
    av_textrec_t *r;

    if (av_ui.n >= AV_TEXT_MAX)
        return;
    r = &av_ui.rec[av_ui.n++];
    r->x = (short)x;
    r->y = (short)y;
    r->ch = (unsigned char)ch;
    r->ctx = (unsigned char)ctx;
    r->ink = (unsigned char)ink;
    r->pad = 0;
}

static void
add_string(int x, int y, const char *s, int ctx, int ink)
{
    for (; *s; s++, x += 8)
        add(x, y, (unsigned char)*s, ctx, ink);
}

/*
 * The font's characters, as plain text.  Below 32 and from 128 to 159 the
 * font holds pictures: line pieces, brackets, the slider, cursors.
 */
static int
map_char(int num, int *ink)
{
    int c = num & 255;

    *ink = AV_INK_NORMAL;
    switch (c) {
    case 128: return '[';           /* slider, left end */
    case 129: return '=';           /* slider bar */
    case 130: return ']';           /* slider, right end */
    case 131: *ink = AV_INK_CURSOR; return '#';  /* slider knob */
    case 12: case 13: *ink = AV_INK_CURSOR; return '>';  /* menu cursor */
    case 10: case 11: *ink = AV_INK_CURSOR; return '_';  /* typing cursor */
    }
    if (c >= 128) {
        *ink = AV_INK_ALT;
        c -= 128;
    }
    if (c >= 32 && c < 127)
        return c;
    switch (c) {
    case 0x1d: case 0x1e: case 0x1f: return '-';  /* separators */
    case 16: return '[';
    case 17: return ']';
    case 18: case 19: case 20: case 21: case 22:
    case 23: case 24: case 25: case 26: case 27:
        return '0' + (c - 18);      /* the font's alternate digits */
    case 28: case 5: case 14: case 15: return '.';
    }
    return ' ';
}

int
AV_TextUI_Char(int x, int y, int num)
{
    int ch, ink;

    if (!av_textui.value || !av_draw_ctx)
        return 0;
    ch = map_char(num, &ink);
    if (ch != ' ')
        add(x, y, ch, av_draw_ctx, ink);
    return 1;
}

/* ---- menu pictures ------------------------------------------------------ */

struct piclabel {
    const char *name;
    const char *lines[6];       /* one per menu row, 20 pixels apart */
    int ink;
};

static const struct piclabel piclabels[] = {
    { "gfx/ttl_main.lmp", { "A F T E R S H O C K" }, AV_INK_TITLE },
    { "gfx/ttl_sgl.lmp",  { "SOLO" }, AV_INK_TITLE },
    { "gfx/p_load.lmp",   { "LOAD" }, AV_INK_TITLE },
    { "gfx/p_save.lmp",   { "SAVE" }, AV_INK_TITLE },
    { "gfx/p_option.lmp", { "OPTIONS" }, AV_INK_TITLE },
    { "gfx/p_multi.lmp",  { "ONLINE" }, AV_INK_TITLE },
    { "gfx/ttl_cstm.lmp", { "KEYS" }, AV_INK_TITLE },
    { "gfx/mainmenu.lmp", { "Play", "Online", "Options", "Help",
                            "Credits", "Quit" },
      AV_INK_NORMAL },
    { "gfx/sp_menu.lmp",  { "New game", "Load", "Save" }, AV_INK_NORMAL },
    { "gfx/mp_menu.lmp",  { "Join a game", "Host a game", "Setup" },
      AV_INK_NORMAL },
};

static const char *const help_text[] = {
    "A F T E R S H O C K",
    "",
    "Arrows or W A S D   walk and turn",
    ", and .             step sideways",
    "Ctrl, F or Enter    fire",
    "Space               jump, swim up",
    "Shift               walk slowly",
    "Page Up, Page Down  look up, down",
    "End or V            look ahead",
    "1 to 8, Q, [ and ]  weapons",
    "Tab                 level score",
    "F6, F9              quick save, load",
    "Esc                 menu",
    "F10 or Ctrl-Q       quit",
    "",
    "Shots find what you face,",
    "so there is little need to aim up or down.",
    "",
    "Engine: TyrQuake (Kevin Shanahan and",
    "contributors) from id Software's GPL",
    "source, libretro's tyrquake core,",
    "OmaQuake by Raul Dias.",
    "World: the LibreQuake project.",
    NULL
};

int
AV_TextUI_MenuPic(int x, int y, int w, int h, const char *name)
{
    size_t i;
    int j;

    if (!av_textui.value || !name)
        return 0;

    if (!strncmp(name, "gfx/menudot", 11)) {
        add(x + 4, y + 4, '>', AV_CTX_MENU, AV_INK_CURSOR);
        return 1;
    }
    if (!strcmp(name, "gfx/qplaque.lmp"))
        return 1;               /* decoration: nothing in text */
    if (!strncmp(name, "gfx/box_", 8)) {
        /* A text box (the quit prompt, typing a name): blank its cells so
         * the menu underneath does not show through. */
        int bx, by;

        for (by = 0; by < h; by += 8)
            for (bx = 0; bx < w; bx += 8)
                add(x + bx, y + by, AV_CH_BLANK, AV_CTX_MENU, AV_INK_NORMAL);
        return 1;
    }
    if (!strncmp(name, "gfx/help", 8)) {
        int widest = 0;

        for (j = 0; help_text[j]; j++)
            if ((int)strlen(help_text[j]) > widest)
                widest = (int)strlen(help_text[j]);
        /* The title centred, the rest as one left-aligned block. */
        for (j = 0; help_text[j]; j++)
            add_string(j == 0 ? 160 - 4 * (int)strlen(help_text[j])
                              : 160 - 4 * widest,
                       8 + j * 8, help_text[j], AV_CTX_MENU,
                       j == 0 ? AV_INK_TITLE : AV_INK_NORMAL);
        return 1;
    }
    for (i = 0; i < sizeof(piclabels) / sizeof(piclabels[0]); i++) {
        const struct piclabel *p = &piclabels[i];

        if (strcmp(name, p->name))
            continue;
        for (j = 0; j < 6 && p->lines[j]; j++) {
            int len = (int)strlen(p->lines[j]);
            int lx = (p->lines[1] ? x : x + w / 2 - len * 4);

            add_string(lx, y + j * 20 + 6, p->lines[j], AV_CTX_MENU, p->ink);
        }
        return 1;
    }
    return 0;
}

/* ---- the player's state ------------------------------------------------- */

void
AV_TextUI_EndFrame(void)
{
    int items = cl.stats[STAT_ITEMS];
    int aw = cl.stats[STAT_ACTIVEWEAPON];
    static const int weapon_bits[8] = {
        IT_AXE, IT_SHOTGUN, IT_SUPER_SHOTGUN, IT_NAILGUN,
        IT_SUPER_NAILGUN, IT_GRENADE_LAUNCHER, IT_ROCKET_LAUNCHER,
        IT_LIGHTNING
    };
    int i;

    av_ui.in_game = (cls.state == ca_active) && cl.worldmodel != NULL;
    av_ui.demo = cls.demoplayback;
    av_ui.health = cl.stats[STAT_HEALTH];
    av_ui.armor = cl.stats[STAT_ARMOR];
    av_ui.armor_kind = (items & IT_ARMOR3) ? 3 : (items & IT_ARMOR2) ? 2 :
                       (items & IT_ARMOR1) ? 1 : 0;
    av_ui.ammo = cl.stats[STAT_AMMO];
    av_ui.ammo_kind = (items & IT_SHELLS) ? 1 : (items & IT_NAILS) ? 2 :
                      (items & IT_ROCKETS) ? 3 : (items & IT_CELLS) ? 4 : 0;
    av_ui.weapon = 0;
    av_ui.weapons_owned = 0;
    for (i = 0; i < 8; i++) {
        if (aw == weapon_bits[i])
            av_ui.weapon = i + 1;
        if (items & weapon_bits[i])
            av_ui.weapons_owned |= 1 << i;
    }
    av_ui.keys = ((items & IT_KEY1) ? 1 : 0) | ((items & IT_KEY2) ? 2 : 0);
    av_ui.powerups = ((items & IT_QUAD) ? 1 : 0) |
                     ((items & IT_INVULNERABILITY) ? 2 : 0) |
                     ((items & IT_INVISIBILITY) ? 4 : 0) |
                     ((items & IT_SUIT) ? 8 : 0);
    av_ui.shells = cl.stats[STAT_SHELLS];
    av_ui.nails = cl.stats[STAT_NAILS];
    av_ui.rockets = cl.stats[STAT_ROCKETS];
    av_ui.cells = cl.stats[STAT_CELLS];
    av_ui.kills = cl.stats[STAT_MONSTERS];
    av_ui.total_kills = cl.stats[STAT_TOTALMONSTERS];
    av_ui.secrets = cl.stats[STAT_SECRETS];
    av_ui.total_secrets = cl.stats[STAT_TOTALSECRETS];
    av_ui.level_time = (int)cl.time;
    av_ui.completed_time = cl.completed_time;
    strlcpy(av_ui.level, cl.levelname, sizeof(av_ui.level));

    av_ui.intermission = cl.intermission;
    av_ui.showscores = sb_showscores;
    av_ui.paused = cl.paused;
    av_ui.loading = scr_drawloading;
    av_ui.menu = (key_dest == key_menu);
    av_ui.console = (int)scr_con_current;
    av_ui.damage_flash = cl.cshifts[CSHIFT_DAMAGE].percent;
    av_ui.picture = (int)av_picture.value;
}

const av_textui_t *
aftershock_textui(void)
{
    return &av_ui;
}
