/* Aftershock: the libretro host around the statically linked core.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_HOST_H
#define AV_HOST_H

#include <stdint.h>

typedef struct {
    const char *pak;            /* aftershock/pak0.pak */
    const char *state_dir;      /* saves and config; the core adds aftershock/ */
    const char *resolution;     /* "WxH" */
    int sound;                  /* 0: the audio device is never opened */
    /* Engine thread.  want_frame is the presentation throttle, asked
     * before the colour expansion so an unwanted frame costs nothing. */
    int (*want_frame)(void);
    void (*frame)(const uint8_t *rgb, int w, int h, int stride);
} av_host_params;

/* The session log (--log).  Opening twice keeps the first; NULL is no
 * log.  Lines go to the file only, flushed as written, from any thread. */
void av_host_log_open(const char *path);
void av_host_log_close(void);
void av_host_log(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

/* Starts the core; -1 when the game could not be loaded. */
int av_host_start(const av_host_params *p);
double av_host_fps(void);
void av_host_run(void);
void av_host_shutdown(void);

/* Input to the engine (engine thread).  code is a RETROK_* value (the
 * core's key numbers are the same), mods RETROKMOD_*. */
void av_host_key(unsigned code, int down, unsigned mods);
/* Relative motion in engine counts, summed until the core reads it. */
void av_host_mouse_move(int dx, int dy);
/* 0 left, 1 middle, 2 right: held state.  3 to 6 wheel up, down, left,
 * right: a press is seen by exactly one read. */
void av_host_mouse_button(int button, int down);

#endif
