/* Aftershock: the pointer.
 *
 * Two sources.  evdev: a grabbed raw device, unbounded relative motion.
 * The terminal: its own SGR pixel reports, absolute positions inside the
 * window, with the steering band of av_look.c at the edges.  auto tries
 * evdev and falls back to the terminal; asking for evdev by name makes
 * failure an error, never a silent switch.
 *
 * Looking is always on: there is no hold-to-look button, so every button
 * stays free for binding (they reach the engine as MOUSE1 and up whichever
 * source is live).
 *
 * Engine thread, except av_pointer_choose (before the terminal is set up).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_pointer.h"
#include "av_clock.h"
#include "av_evdev.h"
#include "av_term.h"

#include <stdio.h>
#include <string.h>

static av_pointer_opts opts;
static int source = AV_MOUSE_NONE;
static int required;            /* evdev asked for by name */
static int focused = 1;         /* terminals that never report focus */
static int released;            /* Ctrl-G */
static int cell_w = 10, cell_h = 20;
static char fallback_why[512];
static char note[1024];
static void (*buttons)(int button, int down);

int av_pointer_focused(void)
{
    return focused;
}

int av_pointer_source(void)
{
    return source;
}

const char *av_pointer_note(void)
{
    return note;
}

void av_pointer_set_buttons(void (*fn)(int button, int down))
{
    buttons = fn;
}

int av_pointer_choose(const av_pointer_opts *o)
{
    char err[512];

    opts = *o;
    av_look_init(&opts.look);
    fallback_why[0] = 0;
    if (opts.dev && (opts.mode == AV_MOUSE_TERM ||
                     opts.mode == AV_MOUSE_NONE)) {
        fprintf(stderr, "aftershock: --mouse-dev is ignored with "
                "--mouse=%s\n", opts.mode == AV_MOUSE_TERM ? "term" : "none");
        opts.dev = NULL;
    }
    if (opts.mode == AV_MOUSE_NONE) {
        source = AV_MOUSE_NONE;
        return 0;
    }
    if (opts.mode == AV_MOUSE_TERM) {
        source = AV_MOUSE_TERM;
        return 0;
    }
    required = opts.mode == AV_MOUSE_EVDEV || opts.dev != NULL;
    if (av_evdev_open(opts.dev, err, sizeof(err)) == 0) {
        source = AV_MOUSE_EVDEV;
        return 0;
    }
    if (required) {
        fprintf(stderr, "aftershock: mouse: %s\n", err);
        return -1;
    }
    snprintf(fallback_why, sizeof(fallback_why), "%s", err);
    source = AV_MOUSE_TERM;
    return 0;
}

static void make_note(void)
{
    if (source == AV_MOUSE_EVDEV) {
        snprintf(note, sizeof(note), "evdev %s (%s), grabbed, 1:1 (no edge "
                 "band)%s", av_evdev_path(),
                 av_evdev_name()[0] ? av_evdev_name() : "no name",
                 opts.edge_given || opts.turn_given ?
                 "; --mouse-edge and --mouse-turn apply to the terminal "
                 "source only" : "");
    } else if (source == AV_MOUSE_TERM) {
        char band[96];

        if (opts.look.edge > 0)
            snprintf(band, sizeof(band), "edge band %.2f at %g deg/s",
                     opts.look.edge, opts.look.turn);
        else
            snprintf(band, sizeof(band), "no edge band");
        snprintf(note, sizeof(note), "terminal SGR-pixels (modes 1003/1016),"
                 " %s%s%s%s", band,
                 fallback_why[0] ? " (evdev: " : "", fallback_why,
                 fallback_why[0] ? ")" : "");
    } else {
        note[0] = 0;
    }
}

int av_pointer_start(int cw, int ch, int cols, int rows, char *err,
                     size_t errcap)
{
    cell_w = cw > 0 ? cw : 10;
    cell_h = ch > 0 ? ch : 20;
    if (source == AV_MOUSE_EVDEV && av_evdev_grab(1) != 0) {
        if (required) {
            snprintf(err, errcap, "could not grab %s (another program has "
                     "it?)", av_evdev_path());
            av_evdev_close();
            return -1;
        }
        snprintf(fallback_why, sizeof(fallback_why), "could not grab %s",
                 av_evdev_path());
        av_evdev_close();
        source = AV_MOUSE_TERM;
    }
    av_look_extent(cols * cell_w, rows * cell_h);
    if (source == AV_MOUSE_TERM)
        av_term_pointer_on(1, 1);
    else if (source == AV_MOUSE_EVDEV)
        av_term_pointer_on(0, 1);
    make_note();
    return 0;
}

void av_pointer_resize(int cols, int rows)
{
    /* From the cached cell size: asking the terminal again would be a
     * write from the wrong thread. */
    av_look_extent(cols * cell_w, rows * cell_h);
}

static int live(void)
{
    return focused && !released;
}

void av_pointer_report(const av_mouse_report *r)
{
    if (source != AV_MOUSE_TERM || !live())
        return;
    av_look_position(r->x, r->y, av_now());
    /* A drag repeats the held button on each motion report: only the
     * others are transitions. */
    if (!r->motion && r->button >= 0 && buttons) {
        if (r->button >= 3) {
            if (!r->release)
                buttons(r->button, 1);
        } else {
            buttons(r->button, !r->release);
        }
    }
}

static void let_go(void)
{
    int b;

    /* Their releases will never arrive while away. */
    if (buttons)
        for (b = 0; b < 3; b++)
            buttons(b, 0);
    av_look_reset();
}

void av_pointer_focus(int in)
{
    if (in == focused)
        return;
    focused = in;
    if (!in) {
        let_go();
        if (source == AV_MOUSE_EVDEV)
            av_evdev_grab(0);
    } else if (source == AV_MOUSE_EVDEV && !released) {
        av_evdev_grab(1);
    }
}

void av_pointer_toggle_release(void)
{
    released = !released;
    if (released) {
        let_go();
        if (source == AV_MOUSE_EVDEV)
            av_evdev_grab(0);
    } else if (source == AV_MOUSE_EVDEV && focused) {
        av_evdev_grab(1);
    }
}

static void evdev_button(int b, int down)
{
    if (live() && buttons)
        buttons(b, down);
}

void av_pointer_frame(double now, int *mx, int *my, double *fx, double *fy)
{
    if (source == AV_MOUSE_EVDEV) {
        int dx = 0, dy = 0;

        /* Drained while away too, and thrown away, so nothing queued
         * arrives as one lurch on return. */
        if (av_evdev_drain(&dx, &dy, evdev_button) < 0)
            source = AV_MOUSE_NONE;
        if (live())
            av_look_relative(dx, dy);
    }
    if (source == AV_MOUSE_NONE) {
        *mx = *my = 0;
        if (fx)
            *fx = 0;
        if (fy)
            *fy = 0;
        return;
    }
    av_look_frame(now, mx, my, fx, fy);
}

void av_pointer_stop(void)
{
    av_evdev_close();
    av_term_pointer_off();
}
