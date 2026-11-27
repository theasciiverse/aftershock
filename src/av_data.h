/* Aftershock: finding the game data.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_DATA_H
#define AV_DATA_H

#include <stddef.h>

/* explicit_path: the positional argument or AFTERSHOCK_DATA, else NULL.
 * On success writes aftershock/pak0.pak's path (or the explicit pak) to
 * out and returns 0; else prints the report to stderr and returns -1. */
int av_data_find(const char *explicit_path, char *out, size_t cap);

#endif
