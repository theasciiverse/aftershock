/* Aftershock: the terminal (raw mode, screens, queries, output).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AV_TERM_H
#define AV_TERM_H

#include <stddef.h>

/* Checks for a tty, enters raw mode and the alternate screen, negotiates
 * the kitty keyboard protocol and asks for the text area in pixels.
 * Prints the reason to stderr and returns -1 when it cannot start. */
int av_term_init(void);
/* Idempotent; also runs from atexit.  Releases a grabbed pointer first. */
void av_term_restore(void);

/* Key releases are reported (kitty keyboard protocol with event types). */
int av_term_kitty(void);

/* Text area in pixels from setup, 0 when the terminal did not say. */
int av_term_text_pixels(int *w, int *h);

/* Terminal size in cells (80x24 when unknown); returns 1 when it differs
 * from the previous call. */
int av_term_size(int *cols, int *rows);

/* SIGINT, SIGTERM and the quit keys end the program through this flag. */
void av_term_request_quit(void);
int av_term_quit_requested(void);

/* Pointer reporting: sgr_pixels turns on any-motion tracking in SGR pixel
 * coordinates; focus turns on focus in/out reports.  off() undoes
 * whichever are on.  Not to be called while the render thread runs. */
void av_term_pointer_on(int sgr_pixels, int focus);
void av_term_pointer_off(void);

/* Reads what the terminal has sent, up to cap bytes, without blocking
 * (or waiting up to timeout_ms for the first byte).  Returns the count,
 * 0 when nothing came, -1 when input is gone.  On Windows the console's
 * key events come back as the same UTF-8 escape sequences a Unix
 * terminal sends. */
int av_term_read(char *buf, size_t cap, int timeout_ms);

/* Bytes as they are. */
void av_term_write_raw(const char *s, size_t n);
/* Rows separated by LF, drawn from (row, col), 1-based. */
void av_term_write_at(const char *s, size_t n, int row, int col);
/* Default colours, whole screen cleared. */
void av_term_clear(void);

#endif
