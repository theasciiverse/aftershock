/* Aftershock: pointer motion to view motion.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_LOOK_H
#define AV_LOOK_H

typedef struct {
    double sens;        /* engine counts per pixel (or device count) */
    double edge;        /* steering band, fraction of each side; 0 off */
    double turn;        /* steering rate at the very edge, deg/s */
    int invert;         /* invert pitch */
} av_look_params;

void av_look_init(const av_look_params *p);
/* The window in pixels (terminal source). */
void av_look_extent(int w, int h);

/* Terminal source: an absolute pointer position at time now (seconds). */
void av_look_position(int x, int y, double now);
/* evdev source: relative device counts. */
void av_look_relative(int dx, int dy);
/* Focus lost or pointer released: forget the position and what is owed,
 * so nothing done while away arrives as one jump. */
void av_look_reset(void);

/* Once a frame: adds the steering band (terminal source), then hands out
 * the whole counts owed, keeping the fraction.  fx, fy (may be NULL) get
 * the band factors, -1 to 1. */
void av_look_frame(double now, int *mx, int *my, double *fx, double *fy);

/* For --keytest: the last position, 0 when there is none. */
int av_look_last(int *x, int *y);

#endif
