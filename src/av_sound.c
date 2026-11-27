/* Aftershock: sound out through ALSA.
 *
 * The engine makes exactly one frame's worth of audio per run, on the
 * engine thread, so nothing here may wait: a write that does not fit is
 * dropped and counted.  80 ms of latency (drops measured the same at 40
 * and 160 ms; what overflows is a loop stall such as a level load).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_sound.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const char *device_name(void)
{
    const char *d = getenv("AFTERSHOCK_ALSA_DEVICE");

    /* The old name, so existing scripts keep working. */
    if (!d || !*d)
        d = getenv("OMAQUAKE_ALSA_DEVICE");
    if (!d || !*d)
        d = "default";
    return d;
}

#ifdef AV_HAVE_ALSA
#include <alsa/asoundlib.h>

static snd_pcm_t *pcm;
static unsigned long dropped;
static const char *dev;

/* ALSA's default handler prints to stderr, and stderr is the picture. */
static void quiet(const char *file, int line, const char *func, int err,
                  const char *fmt, ...)
{
    (void)file;
    (void)line;
    (void)func;
    (void)err;
    (void)fmt;
}

int av_sound_open(unsigned rate, char *err, size_t errcap)
{
    int e;

    snd_lib_error_set_handler(quiet);
    dev = device_name();
    e = snd_pcm_open(&pcm, dev, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (e < 0) {
        snprintf(err, errcap, "%s: %s", dev, snd_strerror(e));
        pcm = NULL;
        return -1;
    }
    /* Resampling allowed: a card that only does 48 kHz must still play. */
    e = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED,
                           2, rate, 1, 80000);
    if (e < 0) {
        snprintf(err, errcap, "%s: %s", dev, snd_strerror(e));
        snd_pcm_close(pcm);
        pcm = NULL;
        return -1;
    }
    dropped = 0;
    return 0;
}

const char *av_sound_device(void)
{
    return dev ? dev : device_name();
}

void av_sound_write(const int16_t *frames, size_t count)
{
    int recovered = 0;

    if (!pcm)
        return;
    while (count > 0) {
        snd_pcm_sframes_t w = snd_pcm_writei(pcm, frames, count);

        if (w == -EAGAIN) {
            break;
        } else if (w < 0) {
            /* Underrun or suspend: recover quietly, but never loop. */
            if (recovered++ >= 2 || snd_pcm_recover(pcm, (int)w, 1) < 0)
                break;
            continue;
        }
        frames += (size_t)w * 2;
        count -= (size_t)w;
        if (w == 0)
            break;
    }
    dropped += count;
}

unsigned long av_sound_close(void)
{
    if (pcm) {
        /* Drop, not drain: draining would wait out the buffer. */
        snd_pcm_drop(pcm);
        snd_pcm_close(pcm);
        pcm = NULL;
    }
    return dropped;
}

#else

int av_sound_open(unsigned rate, char *err, size_t errcap)
{
    (void)rate;
    snprintf(err, errcap, "built without ALSA support");
    return -1;
}

const char *av_sound_device(void)
{
    return device_name();
}

void av_sound_write(const int16_t *frames, size_t count)
{
    (void)frames;
    (void)count;
}

unsigned long av_sound_close(void)
{
    return 0;
}

#endif
