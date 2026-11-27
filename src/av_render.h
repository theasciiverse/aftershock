/* Aftershock: the presentation thread.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_RENDER_H
#define AV_RENDER_H

#include "av_pictures.h"

#include <stdint.h>

/* The backend must already be initialised with cfg.  From here until
 * av_render_stop() this thread is the only one writing to the terminal. */
int av_render_start(const av_backend *be, const av_config *cfg);
/* Engine thread: copies the frame (RGB888) and returns at once. */
void av_render_submit(const uint8_t *rgb, int w, int h, int stride);
/* Engine thread: new canvas geometry (clear, resize the backend). */
void av_render_reconfigure(const av_config *cfg);
/* Engine thread: another backend (old one shut down, clear, new init). */
void av_render_switch(const av_backend *be, const av_config *cfg);
/* Draws a frame still pending, then joins.  Returns frames dropped. */
unsigned long av_render_stop(void);

#endif
