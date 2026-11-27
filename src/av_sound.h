/* Aftershock: sound out through ALSA.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_SOUND_H
#define AV_SOUND_H

#include <stddef.h>
#include <stdint.h>

/* Opens the device for rate Hz stereo.  0 on success; on failure -1 with
 * the reason in err, and every other call does nothing. */
int av_sound_open(unsigned rate, char *err, size_t errcap);
const char *av_sound_device(void);
/* Interleaved stereo frames; never waits, what does not fit is dropped. */
void av_sound_write(const int16_t *frames, size_t count);
/* Returns the number of frames dropped over the session. */
unsigned long av_sound_close(void);

#endif
