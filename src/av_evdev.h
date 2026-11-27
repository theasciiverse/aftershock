/* Aftershock: a raw pointer from /dev/input (Linux evdev).
 *
 * Reading the wrong event device is keylogging, so a node is classified by
 * its capability bitmaps alone and only a pointer is ever read.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_EVDEV_H
#define AV_EVDEV_H

#include <stddef.h>
#include <stdio.h>

enum {
    AV_EV_POINTER,
    AV_EV_KEYBOARD,         /* never opened for reading */
    AV_EV_KBD_POINTER,      /* a keyboard's own pointer interface */
    AV_EV_NOT_POINTER,
    AV_EV_UNREADABLE
};

typedef struct {
    char path[64];
    char name[128];
    int verdict;            /* AV_EV_* */
    int score;              /* pointers: higher is a likelier mouse */
} av_evdev_node;

const char *av_evdev_verdict(int verdict);

/* Every /dev/input/event<N> in numeric order, classified; returns the
 * count (at most max). */
int av_evdev_scan(av_evdev_node *out, int max);

/* The `--mouse-list` report. */
void av_evdev_list(FILE *f);

/* Opens path, or the best pointer when path is NULL.  On failure returns
 * -1 with the reason in err. */
int av_evdev_open(const char *path, char *err, size_t errcap);
int av_evdev_is_open(void);
const char *av_evdev_path(void);
const char *av_evdev_name(void);

/* EVIOCGRAB on or off; 0 on success. */
int av_evdev_grab(int on);

/* Reads what is queued without blocking: motion adds to *dx and *dy,
 * buttons go to fn (0 left, 1 middle, 2 right, 3 to 6 wheel up, down,
 * left, right; wheels as presses only).  Returns -1 when the device went
 * away; it is then closed. */
int av_evdev_drain(int *dx, int *dy, void (*fn)(int button, int down));

/* Ungrabs and closes; safe to call any time, any number of times. */
void av_evdev_close(void);

#endif
