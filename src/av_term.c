/* Aftershock: the terminal.
 *
 * Raw mode, the alternate screen, the kitty keyboard protocol, the text
 * area query, pointer reporting modes and the write paths.  Everything
 * written here before the render thread starts, or after it stops; while
 * it runs it is the only writer (see av_render.c).
 *
 * Two platform layers under the same escape-code logic: POSIX (termios,
 * Linux and macOS) and the Windows console, switched into VT mode so it
 * speaks the same escape codes both ways.  Only raw mode, reading bytes,
 * writing bytes and the size differ.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_term.h"
#include "av_evdev.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

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

#ifndef _WIN32

/* ---- POSIX: termios ---- */

static struct termios saved;

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

int av_term_read(char *buf, size_t cap, int timeout_ms)
{
    for (;;) {
        ssize_t r;

        if (timeout_ms > 0) {
            struct pollfd p = { STDIN_FILENO, POLLIN, 0 };
            int k = poll(&p, 1, timeout_ms);

            if (k < 0 && errno == EINTR)
                continue;
            if (k <= 0)
                return 0;
        }
        r = read(STDIN_FILENO, buf, cap);
        if (r < 0 && errno == EINTR)
            continue;
        return r < 0 ? -1 : (int)r;
    }
}

static int platform_raw(void)
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
    return 0;
}

static void platform_unraw(void)
{
    if (raw_on) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
        raw_on = 0;
    }
}

static void platform_size(int *c, int *r)
{
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 &&
        ws.ws_row > 0) {
        *c = ws.ws_col;
        *r = ws.ws_row;
    }
}

#else

/* ---- Windows: the console in VT mode ----
 *
 * Output: ENABLE_VIRTUAL_TERMINAL_PROCESSING makes the console interpret
 * the same escape codes, DISABLE_NEWLINE_AUTO_RETURN makes LF a plain
 * line feed as with OPOST off, and the UTF-8 code page passes the block
 * characters through as bytes.
 *
 * Input: ENABLE_VIRTUAL_TERMINAL_INPUT turns keys, and mouse reports once
 * asked for, into the escape sequences a Unix terminal sends; they arrive
 * as key events carrying UTF-16 characters, encoded here to UTF-8 so the
 * parser (av_keys.c) sees the same bytes on every platform.  Processed
 * input is off, so Ctrl+C is a byte like any other, as with ISIG off. */

static HANDLE hin, hout;
static DWORD saved_in, saved_out;
static UINT saved_cp_in, saved_cp_out;
static WCHAR high_half;
static int last_esc;       /* the last byte made was an ESC */

void av_term_write_raw(const char *s, size_t n)
{
    while (n > 0) {
        DWORD w = 0;

        if (!WriteFile(hout, s, (DWORD)n, &w, NULL) || w == 0)
            return;
        s += w;
        n -= w;
    }
}

/* One UTF-16 unit to UTF-8; a surrogate pair is held until both halves
 * are in.  Returns the bytes written (at most 4). */
static size_t utf8_of(WCHAR c, char *o)
{
    unsigned long u = c;

    if (c >= 0xD800 && c <= 0xDBFF) {
        high_half = c;
        return 0;
    }
    if (c >= 0xDC00 && c <= 0xDFFF) {
        if (!high_half)
            return 0;
        u = 0x10000 + (((unsigned long)high_half - 0xD800) << 10) +
            ((unsigned long)c - 0xDC00);
        high_half = 0;
    }
    if (u < 0x80) {
        o[0] = (char)u;
        return 1;
    }
    if (u < 0x800) {
        o[0] = (char)(0xC0 | (u >> 6));
        o[1] = (char)(0x80 | (u & 0x3F));
        return 2;
    }
    if (u < 0x10000) {
        o[0] = (char)(0xE0 | (u >> 12));
        o[1] = (char)(0x80 | ((u >> 6) & 0x3F));
        o[2] = (char)(0x80 | (u & 0x3F));
        return 3;
    }
    o[0] = (char)(0xF0 | (u >> 18));
    o[1] = (char)(0x80 | ((u >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((u >> 6) & 0x3F));
    o[3] = (char)(0x80 | (u & 0x3F));
    return 4;
}

/* The escape sequence a Unix terminal sends for a key that carries no
 * character, or NULL.  Windows' own console in VT input mode sends these
 * itself; other hosts (wine's) parse them into bare virtual keys, which
 * are turned back here so av_keys.c sees the same bytes. */
static const char *vk_sequence(WORD vk)
{
    switch (vk) {
    case VK_ESCAPE: return "\033";
    case VK_UP:     return "\033[A";
    case VK_DOWN:   return "\033[B";
    case VK_RIGHT:  return "\033[C";
    case VK_LEFT:   return "\033[D";
    case VK_HOME:   return "\033[H";
    case VK_END:    return "\033[F";
    case VK_INSERT: return "\033[2~";
    case VK_DELETE: return "\033[3~";
    case VK_PRIOR:  return "\033[5~";
    case VK_NEXT:   return "\033[6~";
    case VK_F1:     return "\033OP";
    case VK_F2:     return "\033OQ";
    case VK_F3:     return "\033OR";
    case VK_F4:     return "\033OS";
    case VK_F5:     return "\033[15~";
    case VK_F6:     return "\033[17~";
    case VK_F7:     return "\033[18~";
    case VK_F8:     return "\033[19~";
    case VK_F9:     return "\033[20~";
    case VK_F10:    return "\033[21~";
    case VK_F11:    return "\033[23~";
    case VK_F12:    return "\033[24~";
    default:        return NULL;
    }
}

/* Takes the key events queued now; returns the bytes they made. */
static size_t drain_events(char *buf, size_t cap, int *gone)
{
    size_t n = 0;

    /* An ESC and the character it belongs with arrive together. */
    last_esc = 0;

    /* Leave events queued rather than split one: the next call takes
     * them.  A key event is at most a pair's 4 bytes per repeat. */
    while (cap - n >= 16) {
        INPUT_RECORD rec;
        DWORD avail = 0, got = 0;
        WORD rep;
        WCHAR c;
        DWORD ctl;

        if (!GetNumberOfConsoleInputEvents(hin, &avail)) {
            *gone = 1;
            break;
        }
        if (avail == 0 || !ReadConsoleInputW(hin, &rec, 1, &got) || got == 0)
            break;
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown)
            continue;
        c = rec.Event.KeyEvent.uChar.UnicodeChar;
        ctl = rec.Event.KeyEvent.dwControlKeyState;
        rep = rec.Event.KeyEvent.wRepeatCount ? rec.Event.KeyEvent.wRepeatCount : 1;
        if (c == 0) {
            const char *seq = vk_sequence(rec.Event.KeyEvent.wVirtualKeyCode);
            size_t len = seq ? strlen(seq) : 0;

            while (seq && rep-- > 0 && cap - n >= len) {
                memcpy(buf + n, seq, len);
                n += len;
                last_esc = 0;
            }
            continue;
        }
        while (rep-- > 0 && cap - n >= 5) {
            /* Alt with a character is ESC and the character, as a Unix
             * terminal sends it; that is also how wine passes on the start
             * of an escape sequence.  AltGr (right Alt with left Ctrl) is
             * a character key, and an ESC just sent is not doubled. */
            if ((ctl & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) &&
                (ctl & (RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED)) !=
                    (RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED) &&
                c != 0x1B && !last_esc)
                buf[n++] = 0x1B;
            n += utf8_of(c, buf + n);
            last_esc = c == 0x1B;
        }
    }
    return n;
}

int av_term_read(char *buf, size_t cap, int timeout_ms)
{
    ULONGLONG until = GetTickCount64() + (ULONGLONG)(timeout_ms > 0 ? timeout_ms : 0);
    int gone = 0;

    for (;;) {
        size_t n = drain_events(buf, cap, &gone);
        ULONGLONG now;

        if (n > 0)
            return (int)n;
        if (gone)
            return -1;
        /* The handle is signalled by any event, key-ups and focus
         * included, which make no bytes: wait again until the deadline. */
        now = GetTickCount64();
        if (timeout_ms <= 0 || now >= until ||
            WaitForSingleObject(hin, (DWORD)(until - now)) != WAIT_OBJECT_0)
            return 0;
    }
}

static BOOL WINAPI on_ctrl(DWORD type)
{
    (void)type;
    /* Close, log-off and shutdown: quit as on SIGTERM.  Windows ends the
     * process a few seconds later whatever we return. */
    quit_flag = 1;
    return TRUE;
}

static int platform_raw(void)
{
    hin = GetStdHandle(STD_INPUT_HANDLE);
    hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!GetConsoleMode(hin, &saved_in) || !GetConsoleMode(hout, &saved_out)) {
        fprintf(stderr, "aftershock: stdin/stdout must be a console\n");
        return -1;
    }
    saved_cp_in = GetConsoleCP();
    saved_cp_out = GetConsoleOutputCP();
    if (!SetConsoleMode(hout, saved_out | ENABLE_PROCESSED_OUTPUT |
                                  ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                                  DISABLE_NEWLINE_AUTO_RETURN)) {
        fprintf(stderr, "aftershock: this console does not understand "
                "escape codes (Windows 10 1809 or newer is needed)\n");
        return -1;
    }
    /* No line input, echo, processed input or quick edit (a click would
     * freeze the program while text is selected). */
    if (!SetConsoleMode(hin, ENABLE_VIRTUAL_TERMINAL_INPUT |
                                 ENABLE_EXTENDED_FLAGS)) {
        SetConsoleMode(hout, saved_out);
        fprintf(stderr, "aftershock: this console cannot send escape codes "
                "as input (Windows 10 1809 or newer is needed)\n");
        return -1;
    }
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    FlushConsoleInputBuffer(hin);
    raw_on = 1;
    atexit(av_term_restore);
    SetConsoleCtrlHandler(on_ctrl, TRUE);
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    return 0;
}

static void platform_unraw(void)
{
    if (raw_on) {
        SetConsoleMode(hin, saved_in);
        SetConsoleMode(hout, saved_out);
        SetConsoleCP(saved_cp_in);
        SetConsoleOutputCP(saved_cp_out);
        raw_on = 0;
    }
}

static void platform_size(int *c, int *r)
{
    CONSOLE_SCREEN_BUFFER_INFO b;

    if (GetConsoleScreenBufferInfo(hout, &b)) {
        int w = b.srWindow.Right - b.srWindow.Left + 1;
        int h = b.srWindow.Bottom - b.srWindow.Top + 1;

        if (w > 0 && h > 0) {
            *c = w;
            *r = h;
        }
    }
}

#endif

/* ---- shared: escape codes ---- */


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
        int r = av_term_read(buf + len, cap - 1 - len, 200);
        size_t i;
        int done = 0;

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
    if (platform_raw() != 0)
        return -1;

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
    platform_unraw();
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
    int c = 80, r = 24, changed;

    platform_size(&c, &r);
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
