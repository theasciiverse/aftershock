/* Aftershock: sound out through miniaudio (macOS: Core Audio; Windows:
 * WASAPI, then DirectSound or WinMM).  Linux keeps ALSA (av_sound.c).
 *
 * The same contract as av_sound.c: the engine makes one frame's worth of
 * audio per run, on the engine thread, and nothing here may wait.  The
 * engine thread writes into a ring and the device's own thread reads it;
 * a write that does not fit is dropped and counted, a read that finds the
 * ring short plays silence for the rest.  The ring holds 80 ms, ALSA's
 * buffer on Linux.
 *
 * One writer and one reader, so the two positions need only be atomic.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_sound.h"
#include "av_miniaudio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ma_device device;
static int open_ok;
static int16_t *ring;          /* stereo frames */
static size_t ring_frames;
static size_t head, tail;      /* written, read; frame counts, wrapping */
static unsigned long dropped;
static char dev[96] = "default";

static size_t load(const size_t *p)
{
    return __atomic_load_n(p, __ATOMIC_ACQUIRE);
}

static void store(size_t *p, size_t v)
{
    __atomic_store_n(p, v, __ATOMIC_RELEASE);
}

/* The device thread. */
static void pull(ma_device *d, void *out, const void *in, ma_uint32 count)
{
    int16_t *o = out;
    size_t t = load(&tail), have = load(&head) - t, n = count, i;

    (void)d;
    (void)in;
    if (n > have)
        n = have;
    for (i = 0; i < n; i++) {
        size_t at = (t + i) % ring_frames;

        o[2 * i] = ring[2 * at];
        o[2 * i + 1] = ring[2 * at + 1];
    }
    memset(o + 2 * n, 0, (count - n) * 2 * sizeof(int16_t));
    store(&tail, t + n);
}

int av_sound_open(unsigned rate, char *err, size_t errcap)
{
    ma_device_config c;
    ma_result r;

    ring_frames = rate * 80 / 1000;
    ring = calloc(ring_frames, 2 * sizeof(int16_t));
    if (!ring) {
        snprintf(err, errcap, "out of memory");
        return -1;
    }
    head = tail = 0;
    dropped = 0;

    c = ma_device_config_init(ma_device_type_playback);
    c.playback.format = ma_format_s16;
    c.playback.channels = 2;
    c.sampleRate = rate;
    c.dataCallback = pull;
    /* Short periods: the ring is the buffer. */
    c.periodSizeInMilliseconds = 10;
    r = ma_device_init(NULL, &c, &device);
    if (r != MA_SUCCESS) {
        snprintf(err, errcap, "no sound device (%s)", ma_result_description(r));
        free(ring);
        ring = NULL;
        return -1;
    }
    snprintf(dev, sizeof(dev), "%s: %s",
             ma_get_backend_name(device.pContext->backend),
             device.playback.name);
    r = ma_device_start(&device);
    if (r != MA_SUCCESS) {
        snprintf(err, errcap, "%s: %s", dev, ma_result_description(r));
        ma_device_uninit(&device);
        free(ring);
        ring = NULL;
        return -1;
    }
    open_ok = 1;
    return 0;
}

const char *av_sound_device(void)
{
    return dev;
}

/* The engine thread. */
void av_sound_write(const int16_t *frames, size_t count)
{
    size_t h, room, n, i;

    if (!open_ok)
        return;
    h = load(&head);
    room = ring_frames - (h - load(&tail));
    n = count < room ? count : room;
    for (i = 0; i < n; i++) {
        size_t at = (h + i) % ring_frames;

        ring[2 * at] = frames[2 * i];
        ring[2 * at + 1] = frames[2 * i + 1];
    }
    store(&head, h + n);
    dropped += count - n;
}

unsigned long av_sound_close(void)
{
    if (open_ok) {
        ma_device_uninit(&device);
        open_ok = 0;
        free(ring);
        ring = NULL;
    }
    return dropped;
}
