/* Aftershock: terminal bytes to key, pointer and focus events.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_KEYS_H
#define AV_KEYS_H

#include <stddef.h>

typedef struct {
    int button;     /* -1 none, 0 left, 1 middle, 2 right, 3..6 wheel
                       up, down, left, right */
    int x, y;       /* pixels (mode 1016), may be negative */
    int motion;     /* a motion report, not a button transition */
    int release;    /* final byte m */
} av_mouse_report;

typedef struct {
    /* What the engine gets: RETROK_* code, down, RETROKMOD_* mods.  On a
     * terminal without key releases, holds are synthesised before this. */
    void (*key)(unsigned code, int down, unsigned mods);
    /* Nonzero while a menu, the console or a message line has the
     * keyboard (presses then become press and release at once). */
    int (*text_input)(void);
    void (*mouse)(const av_mouse_report *r);
    void (*focus)(int in);
    /* Ctrl-G: release or take back the pointer. */
    void (*release_toggle)(void);
    /* Diagnostics (--keytest); any may be NULL. */
    void (*raw)(const unsigned char *bytes, size_t n);
    void (*decoded)(unsigned code, int down, unsigned mods);
    void (*x10)(void);
} av_keys_sink;

void av_keys_init(const av_keys_sink *sink, int kitty);
/* Reads everything stdin has and decodes it. */
void av_keys_poll(void);
/* Ends synthesised holds whose time is up; every loop iteration. */
void av_keys_expire(void);
/* A name for a RETROK code, for --keytest. */
const char *av_keys_name(unsigned code);

#endif
