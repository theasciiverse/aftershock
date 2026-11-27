/* Aftershock: the pointer (which source, focus, release, the per-frame
 * step).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_POINTER_H
#define AV_POINTER_H

#include "av_keys.h"
#include "av_look.h"

#include <stddef.h>

enum { AV_MOUSE_AUTO, AV_MOUSE_EVDEV, AV_MOUSE_TERM, AV_MOUSE_NONE };

typedef struct {
    int mode;               /* AV_MOUSE_* */
    const char *dev;        /* --mouse-dev, or NULL */
    av_look_params look;
    int edge_given, turn_given;
} av_pointer_opts;

/* Before the terminal is touched, so an error reaches a plain stderr.
 * Returns -1 (message printed) when a required evdev device cannot be
 * opened. */
int av_pointer_choose(const av_pointer_opts *o);

/* buttons: 0 left, 1 middle, 2 right, 3..6 wheel; where they go. */
void av_pointer_set_buttons(void (*fn)(int button, int down));

/* Just before the loop: grab, calibrate (cell size in pixels and the
 * terminal in cells), turn on the reporting the source needs.  Returns -1
 * with the reason in err when a required grab fails. */
int av_pointer_start(int cell_w, int cell_h, int cols, int rows,
                     char *err, size_t errcap);
/* Terminal resized: the extent from the new cell count. */
void av_pointer_resize(int cols, int rows);

/* From the key decoder. */
void av_pointer_report(const av_mouse_report *r);
void av_pointer_focus(int in);
void av_pointer_toggle_release(void);

/* Once a frame: the counts to hand the engine; fx, fy the band factors. */
void av_pointer_frame(double now, int *mx, int *my, double *fx, double *fy);

/* Ungrab and close the device, turn reporting off. */
void av_pointer_stop(void);

int av_pointer_focused(void);
/* AV_MOUSE_EVDEV, AV_MOUSE_TERM or AV_MOUSE_NONE once chosen. */
int av_pointer_source(void);
/* One line for the log and the exit message; "" when there is none. */
const char *av_pointer_note(void);

#endif
