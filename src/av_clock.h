/* Aftershock: monotonic seconds, for pacing, holds and the look model.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_CLOCK_H
#define AV_CLOCK_H

#include <time.h>

static inline double av_now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

#endif
