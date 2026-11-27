/* Aftershock: pointer motion to view motion.
 *
 * The engine reads relative mouse counts and multiplies them as integers
 * (one count is 0.066 degrees with stock settings: sensitivity 3 times
 * m_yaw 0.022), so scaling happens here, before it, and the fractions of
 * a count are kept for the next frame.
 *
 * A terminal reports absolute pixel positions inside its window, with no
 * pointer lock and no warp, so the view would stop at the window's edge.
 * Hence the steering band: near each side the view keeps turning, faster
 * the closer the pointer is to the edge.  1:1 motion and the band add up,
 * so a turn carries on smoothly as the pointer runs out of room.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_look.h"

#include <math.h>

#define DEG_PER_COUNT 0.066
#define RESYNC 0.150            /* s: an older position is not a motion */
#define MAX_JUMP 500            /* px: more is a mis-associated report */
#define MAX_HAND 30000

static av_look_params lp = { 2.0, 0.15, 220.0, 0 };
static int ext_w, ext_h;
static int have_pos, pos_x, pos_y;
static double pos_t, last_frame;
static double acc_x, acc_y;

void av_look_init(const av_look_params *p)
{
    lp = *p;
    av_look_reset();
}

void av_look_extent(int w, int h)
{
    ext_w = w;
    ext_h = h;
}

void av_look_reset(void)
{
    have_pos = 0;
    acc_x = acc_y = 0;
    last_frame = 0;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

void av_look_position(int x, int y, double now)
{
    /* Left on one side and came back on the other: not one huge flick. */
    if (have_pos && now - pos_t < RESYNC) {
        int dx = clampi(x - pos_x, -MAX_JUMP, MAX_JUMP);
        int dy = clampi(y - pos_y, -MAX_JUMP, MAX_JUMP);

        acc_x += dx * lp.sens;
        acc_y += (lp.invert ? -dy : dy) * lp.sens;
    }
    have_pos = 1;
    pos_x = x;
    pos_y = y;
    pos_t = now;
}

void av_look_relative(int dx, int dy)
{
    acc_x += dx * lp.sens;
    acc_y += (lp.invert ? -dy : dy) * lp.sens;
}

/* -1 at the low edge to 1 at the high one, 0 outside the band, squared
 * with its sign kept so entering the band is a nudge. */
static double band(int pos, int extent)
{
    double b, t;

    if (lp.edge <= 0 || extent <= 0)
        return 0;
    b = lp.edge * extent;
    if (b < 1)
        b = 1;
    if (pos < b)
        t = -(b - pos) / b;
    else if (pos > extent - b)
        t = (pos - (extent - b)) / b;
    else
        return 0;
    if (t > 1)
        t = 1;
    if (t < -1)
        t = -1;
    return t * fabs(t);
}

void av_look_frame(double now, int *mx, int *my, double *fx, double *fy)
{
    double bx = 0, by = 0, dt;
    long hx, hy;

    if (have_pos) {
        bx = band(pos_x, ext_w);
        by = band(pos_y, ext_h);
        /* A stall is not paid back as a lurch. */
        dt = last_frame > 0 ? now - last_frame : 0;
        if (dt < 0)
            dt = 0;
        if (dt > 0.1)
            dt = 0.1;
        acc_x += bx * (lp.turn / DEG_PER_COUNT) * dt;
        acc_y += (lp.invert ? -by : by) * (lp.turn / DEG_PER_COUNT) * dt;
    }
    last_frame = now;
    if (fx)
        *fx = bx;
    if (fy)
        *fy = by;

    /* Whole counts, toward zero; a slow steer is a fraction of a count a
     * frame and would otherwise never move. */
    hx = (long)acc_x;
    hy = (long)acc_y;
    acc_x -= (double)hx;
    acc_y -= (double)hy;
    if (hx > MAX_HAND) hx = MAX_HAND;
    if (hx < -MAX_HAND) hx = -MAX_HAND;
    if (hy > MAX_HAND) hy = MAX_HAND;
    if (hy < -MAX_HAND) hy = -MAX_HAND;
    *mx = (int)hx;
    *my = (int)hy;
}

int av_look_last(int *x, int *y)
{
    *x = pos_x;
    *y = pos_y;
    return have_pos;
}
