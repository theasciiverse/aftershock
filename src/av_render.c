/* Aftershock: the presentation thread.
 *
 * The engine alone takes about one 60 Hz frame of CPU, so converting a
 * frame to escape codes and writing them runs here, on a second thread.
 * Measured before this design: 11.8 fps at 400x100 cells (1.1 MB of escape
 * codes a frame); after, about 61 fps at any size with no audio drops.
 *
 * The engine never waits for the terminal: a submitted frame overwrites
 * one that was never drawn (newest wins; a stale frame is worth nothing).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_render.h"
#include "av_overlay.h"
#include "av_term.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *px;
    size_t cap;
    int w, h, stride;
} frame_buf;

static pthread_t thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake = PTHREAD_COND_INITIALIZER;
static int running, started;
static frame_buf pending, drawing;
static int have_pending;
static unsigned long dropped;

/* Under lock: what the engine asked for, taken by the thread. */
static av_config want_cfg;
static const av_backend *want_be;
static int want_reconf, want_switch;

/* The thread's own. */
static const av_backend *be;
static av_config cfg;

static void apply_requests(const av_backend *nbe, const av_config *ncfg,
                           int sw, int reconf)
{
    if (sw && nbe != be) {
        be->shutdown();
        av_term_clear();
        be = nbe;
        cfg = *ncfg;
        be->init(&cfg);
    } else if (sw || reconf) {
        /* The same backend with other symbols is a reconfigure. */
        cfg = *ncfg;
        av_term_clear();
        be->resize(&cfg);
    }
}

static void draw(const frame_buf *f)
{
    be->frame(f->px, f->w, f->h, f->stride);
    /* Our half-block family lays out the text itself. */
    if (!av_half_family(be))
        av_overlay_emit(cfg.cols, cfg.rows + av_overlay_reserved_rows(),
                        cfg.origin_col, cfg.origin_row);
}

static void *render_loop(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&lock);
    for (;;) {
        const av_backend *nbe;
        av_config ncfg;
        int sw, reconf, have;
        frame_buf t;

        while (running && !have_pending && !want_reconf && !want_switch)
            pthread_cond_wait(&wake, &lock);
        if (!running && !have_pending && !want_reconf && !want_switch)
            break;

        have = have_pending;
        if (have) {
            t = drawing;
            drawing = pending;
            pending = t;
            have_pending = 0;
        }
        nbe = want_be;
        ncfg = want_cfg;
        sw = want_switch;
        reconf = want_reconf;
        want_switch = want_reconf = 0;
        pthread_mutex_unlock(&lock);

        apply_requests(nbe, &ncfg, sw, reconf);
        if (have)
            draw(&drawing);

        pthread_mutex_lock(&lock);
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

int av_render_start(const av_backend *b, const av_config *c)
{
    be = b;
    cfg = *c;
    want_be = b;
    want_cfg = *c;
    want_reconf = want_switch = 0;
    have_pending = 0;
    dropped = 0;
    running = 1;
    if (pthread_create(&thread, NULL, render_loop, NULL) != 0) {
        running = 0;
        return -1;
    }
    started = 1;
    return 0;
}

void av_render_submit(const uint8_t *rgb, int w, int h, int stride)
{
    size_t need = (size_t)w * h * 3;
    int y;

    pthread_mutex_lock(&lock);
    if (pending.cap < need) {
        free(pending.px);
        pending.px = malloc(need);
        pending.cap = pending.px ? need : 0;
        if (!pending.px) {
            have_pending = 0;
            pthread_mutex_unlock(&lock);
            return;
        }
    }
    for (y = 0; y < h; y++)
        memcpy(pending.px + (size_t)y * w * 3, rgb + (size_t)y * stride,
               (size_t)w * 3);
    pending.w = w;
    pending.h = h;
    pending.stride = w * 3;
    if (have_pending)
        dropped++;
    have_pending = 1;
    pthread_cond_signal(&wake);
    pthread_mutex_unlock(&lock);
}

void av_render_reconfigure(const av_config *c)
{
    pthread_mutex_lock(&lock);
    want_cfg = *c;
    want_reconf = 1;
    pthread_cond_signal(&wake);
    pthread_mutex_unlock(&lock);
}

void av_render_switch(const av_backend *b, const av_config *c)
{
    pthread_mutex_lock(&lock);
    want_be = b;
    want_cfg = *c;
    want_switch = 1;
    pthread_cond_signal(&wake);
    pthread_mutex_unlock(&lock);
}

unsigned long av_render_stop(void)
{
    if (!started)
        return dropped;
    pthread_mutex_lock(&lock);
    running = 0;
    pthread_cond_signal(&wake);
    pthread_mutex_unlock(&lock);
    pthread_join(thread, NULL);
    started = 0;
    free(pending.px);
    free(drawing.px);
    memset(&pending, 0, sizeof(pending));
    memset(&drawing, 0, sizeof(drawing));
    return dropped;
}
