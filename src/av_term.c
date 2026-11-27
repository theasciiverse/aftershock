/* Aftershock: the terminal.
 *
 * Raw mode, the alternate screen, the kitty keyboard protocol, the text
 * area query, pointer reporting modes and the write paths.  Everything
 * written here before the render thread starts, or after it stops; while
 * it runs it is the only writer (see av_render.c).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_term.h"
#include "av_evdev.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static struct termios saved;
static int raw_on, screen_on, kitty_pushed, kitty_events;
static int ptr_sgr, ptr_focus;
static int px_w, px_h;
static int last_cols = -1, last_rows = -1;
static volatile sig_atomic_t quit_flag;

void av_term_request_quit(void)
{
    quit_flag = 1;
}

int av_term_quit_requested(void)
{
    return quit_flag != 0;
}

static void on_signal(int sig)
{
    (void)sig;
    quit_flag = 1;
}

void av_term_write_raw(const char *s, size_t n)
{
    while (n > 0) {
        ssize_t w = write(STDOUT_FILENO, s, n);

        if (w < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        s += w;
        n -= (size_t)w;
    }
}

static void say(const char *s)
{
    av_term_write_raw(s, strlen(s));
}

/* Sends a query followed by Primary Device Attributes and collects the
 * replies until the DA answer arrives.  Every terminal answers DA, so once
 * it is in, the wanted reply has either come before it or never will.  A
 * terminal that answers nothing at all costs one 200 ms timeout. */
static size_t ask(const char *query, char *buf, size_t cap)
{
    size_t len = 0;

    say(query);
    say("\033[c");
    while (len + 1 < cap) {
        struct pollfd p = { STDIN_FILENO, POLLIN, 0 };
        ssize_t r;
        size_t i;
        int done = 0;

        if (poll(&p, 1, 200) <= 0)
            break;
        r = read(STDIN_FILENO, buf + len, cap - 1 - len);
        if (r < 0 && errno == EINTR)
            continue;
        if (r <= 0)
            break;
        len += (size_t)r;
        buf[len] = 0;
        /* A DA reply: ESC [ ? digits and semicolons, then c. */
        for (i = 0; i + 2 < len && !done; i++) {
            size_t j;

            if (buf[i] != '\033' || buf[i + 1] != '[' || buf[i + 2] != '?')
                continue;
            for (j = i + 3; j < len; j++) {
                if (buf[j] == 'c') {
                    done = 1;
                    break;
                }
                if (!((buf[j] >= '0' && buf[j] <= '9') || buf[j] == ';'))
                    break;
            }
        }
        if (done)
            break;
    }
    buf[len] = 0;
    return len;
}

/* The kitty flags from a reply, ESC [ ? digits u, or -1.  The DA reply
 * starts the same way but ends in c. */
static int kitty_flags(const char *buf, size_t len)
{
    size_t i, j;

    for (i = 0; i + 3 < len; i++) {
        int v = 0;

        if (buf[i] != '\033' || buf[i + 1] != '[' || buf[i + 2] != '?')
            continue;
        for (j = i + 3; j < len && buf[j] >= '0' && buf[j] <= '9'; j++)
            v = v * 10 + (buf[j] - '0');
        if (j > i + 3 && j < len && buf[j] == 'u')
            return v;
    }
    return -1;
}

static void negotiate_kitty(void)
{
    char buf[512];
    size_t n;

    n = ask("\033[?u", buf, sizeof(buf));
    if (kitty_flags(buf, n) < 0)
        return;
    /* 1 disambiguate, 2 event types, 8 every key as an escape code. */
    say("\033[>11u");
    kitty_pushed = 1;
    n = ask("\033[?u", buf, sizeof(buf));
    if (kitty_flags(buf, n) >= 0 && (kitty_flags(buf, n) & 2)) {
        kitty_events = 1;
        return;
    }
    /* Pushed but no event types: legacy bytes it is, and the stack goes
     * back as it was. */
    say("\033[<u");
    kitty_pushed = 0;
}

/* Must run while stdin is still ours: once the input parser owns it, a
 * late reply would be decoded as keys. */
static void query_pixels(void)
{
    char buf[512];
    size_t n = ask("\033[14t", buf, sizeof(buf)), i;

    for (i = 0; i + 3 < n; i++) {
        int h = 0, w = 0;
        char t = 0;

        if (buf[i] != '\033' || buf[i + 1] != '[' || buf[i + 2] != '4' ||
            buf[i + 3] != ';')
            continue;
        if (sscanf(buf + i + 4, "%d;%d%c", &h, &w, &t) == 3 && t == 't' &&
            w > 0 && h > 0) {
            px_w = w;
            px_h = h;
            return;
        }
    }
}

int av_term_init(void)
{
    struct termios t;
    struct sigaction sa;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        fprintf(stderr, "aftershock: stdin/stdout must be a terminal\n");
        return -1;
    }
    if (tcgetattr(STDIN_FILENO, &saved) != 0) {
        perror("aftershock: tcgetattr");
        return -1;
    }
    t = saved;
    t.c_iflag &= ~(tcflag_t)(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    t.c_oflag &= ~(tcflag_t)OPOST;
    t.c_lflag &= ~(tcflag_t)(ECHO | ICANON | ISIG | IEXTEN);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) != 0) {
        perror("aftershock: tcsetattr");
        return -1;
    }
    raw_on = 1;
    atexit(av_term_restore);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    /* Size is polled each frame, so a resize needs no handler. */
    signal(SIGWINCH, SIG_IGN);

    /* Alternate screen, no cursor, no autowrap (a glyph in the last
     * column must not wrap and scroll), clear. */
    say("\033[?1049h\033[?25l\033[?7l\033[2J");
    screen_on = 1;

    /* Only now: kitty's flag stack is per screen, so flags pushed on the
     * main screen are gone on this one, and every key would then stick
     * down waiting for a release that never comes. */
    negotiate_kitty();
    query_pixels();
    return 0;
}

void av_term_pointer_on(int sgr_pixels, int focus)
{
    if (sgr_pixels && !ptr_sgr) {
        /* 1003: every motion, so looking needs no button held.  1016:
         * pixel coordinates; only in this mode do terminals such as
         * Ghostty stop coalescing motion to one report per cell. */
        say("\033[?1003h\033[?1016h");
        ptr_sgr = 1;
    }
    if (focus && !ptr_focus) {
        say("\033[?1004h");
        ptr_focus = 1;
    }
}

void av_term_pointer_off(void)
{
    if (ptr_sgr)
        say("\033[?1016l\033[?1003l");
    if (ptr_focus)
        say("\033[?1004l");
    ptr_sgr = ptr_focus = 0;
}

void av_term_restore(void)
{
    /* A grabbed pointer left on is the whole desktop's pointer frozen. */
    av_evdev_close();
    if (screen_on) {
        av_term_pointer_off();
        if (kitty_pushed)
            say("\033[<u");
        kitty_pushed = kitty_events = 0;
        say("\033[?7h\033[?25h\033[0m\033[?1049l");
        screen_on = 0;
    }
    if (raw_on) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
        raw_on = 0;
    }
}

int av_term_kitty(void)
{
    return kitty_events;
}

int av_term_text_pixels(int *w, int *h)
{
    *w = px_w;
    *h = px_h;
    return px_w > 0 && px_h > 0;
}

int av_term_size(int *cols, int *rows)
{
    struct winsize ws;
    int c = 80, r = 24, changed;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 &&
        ws.ws_row > 0) {
        c = ws.ws_col;
        r = ws.ws_row;
    }
    changed = c != last_cols || r != last_rows;
    last_cols = c;
    last_rows = r;
    *cols = c;
    *rows = r;
    return changed;
}

void av_term_write_at(const char *s, size_t n, int row, int col)
{
    size_t lines = 1, i, o = 0, cap;
    char *buf;
    int line = 0;

    if (row < 1)
        row = 1;
    if (col < 1)
        col = 1;
    for (i = 0; i < n; i++)
        if (s[i] == '\n')
            lines++;
    cap = n + lines * 24 + 16;
    buf = malloc(cap);
    if (!buf)
        return;
    o += (size_t)snprintf(buf, cap, "\033[%d;%dH", row, col);
    for (i = 0; i < n; i++) {
        if (s[i] == '\r' && i + 1 < n && s[i + 1] == '\n')
            continue;
        if (s[i] != '\n') {
            buf[o++] = s[i];
            continue;
        }
        line++;
        /* OPOST is off, so a bare LF keeps the column: without the CR
         * (or a cursor move) the picture tears into diagonal bands. */
        if (col == 1) {
            buf[o++] = '\r';
            buf[o++] = '\n';
        } else {
            o += (size_t)snprintf(buf + o, cap - o, "\033[%d;%dH",
                                  row + line, col);
        }
    }
    av_term_write_raw(buf, o);
    free(buf);
}

void av_term_clear(void)
{
    say("\033[0m\033[2J");
}
