/*
 * Aftershock: the game's text, handed to the terminal as text.
 *
 * Copyright (C) 2026 the Aftershock authors.  GPL-2.0-or-later, as the
 * engine.
 *
 * At 100x30 the engine's 8-pixel font and its picture status bar are a few
 * blurred pixels.  With av_textui 1 (the default) the engine does not draw
 * them; it records each character it would have drawn (menus, console,
 * centre messages) and fills in a summary of the player's state, and the
 * terminal frontend draws all of it as real characters over the picture.
 *
 * This header is self-contained so the frontend can include it.
 */
#ifndef AV_TEXTUI_H
#define AV_TEXTUI_H

#define AV_TEXT_MAX 6144

enum {
    AV_CTX_NONE = 0,    /* drawn as pixels, as before */
    AV_CTX_MENU,        /* x, y in the menus' 320x200 space */
    AV_CTX_CONSOLE,     /* screen pixels, top left anchored */
    AV_CTX_CENTER,      /* screen pixels, centred */
};

/* A record with this character blanks its cell (a text box's backing). */
#define AV_CH_BLANK 1

/* Colour hints for a record. */
enum {
    AV_INK_NORMAL = 0,  /* light text */
    AV_INK_ALT,         /* the font's second colour (gold) */
    AV_INK_TITLE,       /* menu titles */
    AV_INK_CURSOR,      /* the menu cursor */
    AV_INK_DIM,         /* greyed out */
};

typedef struct {
    short x, y;
    unsigned char ch;   /* plain ASCII after mapping */
    unsigned char ctx;
    unsigned char ink;
    unsigned char pad;
} av_textrec_t;

typedef struct {
    int vid_w, vid_h;
    int n;
    av_textrec_t rec[AV_TEXT_MAX];

    /* the player */
    int in_game;            /* a level is running and the HUD applies */
    int demo;               /* it is a recorded demo (the attract mode) */
    int health, armor, armor_kind;   /* kind 0 none, 1 green, 2 yellow, 3 red */
    int ammo, ammo_kind;    /* kind 0 none, 1 shells, 2 nails, 3 rockets, 4 cells */
    int weapon;             /* 1..8 selected, 0 none */
    int weapons_owned;      /* bit (n-1) for weapon n */
    int keys;               /* bit 0 silver, bit 1 gold */
    int powerups;           /* bit 0 sigil, 1 aegis, 2 eye, 3 suit */
    int shells, nails, rockets, cells;
    int kills, total_kills, secrets, total_secrets;
    int level_time;         /* seconds */
    int completed_time;     /* seconds, at the end of a level */
    char level[64];

    /* what is on screen */
    int intermission;       /* 0, 1 stats, 2 finale, 3 cutscene */
    int showscores;
    int paused;
    int loading;
    int menu;               /* a menu is up */
    int console;            /* rows of console shown, in pixels */
    int damage_flash;       /* recently hurt: 0..255 */

    /* how the frontend draws the picture: 0 half blocks, 1 sextants
     * (av_picture, chosen in Options, Video) */
    int picture;
} av_textui_t;

/* Set by the frontend before the core starts when --video named a picture:
 * 0 half blocks, 1 sextants, -1 (the default) keeps the saved choice. */
extern int aftershock_picture_arg;

#ifndef AV_TEXTUI_FRONTEND
#include "cvar.h"

extern cvar_t av_textui;
extern cvar_t av_picture;
extern av_textui_t av_ui;
extern int av_draw_ctx;

void AV_TextUI_Init(void);
void AV_TextUI_BeginFrame(void);
void AV_TextUI_EndFrame(void);
/* Records a character if text mode has the current context; returns 1 when
 * it did, and the caller then draws nothing. */
int AV_TextUI_Char(int x, int y, int num);
/* Menu pictures: draws a text label in place of a known picture and returns
 * 1, or returns 0 to let the picture be drawn. */
int AV_TextUI_MenuPic(int x, int y, int w, int h, const char *name);
#endif

/* Frontend: the snapshot of the last finished frame (engine thread). */
const av_textui_t *aftershock_textui(void);

#endif
