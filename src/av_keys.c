/* Aftershock: terminal bytes to key, pointer and focus events.
 *
 * Two kinds of terminal: one speaking the kitty keyboard protocol, which
 * reports every key as an escape sequence with presses and releases, and
 * the rest, which send bytes on press only.  For the second kind a held
 * key is guessed from the terminal's auto-repeat (see the holds below).
 *
 * Runs on the engine thread.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_keys.h"
#include "av_clock.h"
#include "av_term.h"

#include <libretro.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Pointer reports under any-motion tracking arrive fast: a 1 KB buffer
 * overflowed and wedged. */
#define BUF_SIZE 32768

static av_keys_sink sink;
static int kitty;
static unsigned char buf[BUF_SIZE];
static size_t len;
static double esc_since;            /* 0: no lone ESC waiting */

/* Keys the frontend took (quit, Ctrl-G): their releases are not passed on
 * either, whatever the modifiers are by then. */
static unsigned char swallowed[RETROK_LAST];

/* ---- synthesised holds (no key releases) ----
 *
 * doom-ascii's timings (David's choice; a flat 260 ms made walking stutter
 * in terminals with a long repeat delay): a fresh press is held 550 ms,
 * which bridges the first auto-repeat delay; each repeat while held
 * extends it to 100 ms from now. */
#define HOLD_FIRST 0.550
#define HOLD_REPEAT 0.100
#define MAX_HOLDS 16

static struct {
    unsigned code, mods;
    double until;
} holds[MAX_HOLDS];
static int nholds;

static void to_engine(unsigned code, int down, unsigned mods)
{
    if (sink.key)
        sink.key(code, down, mods);
}

static void hold_press(unsigned code, unsigned mods)
{
    double now = av_now();
    int i, soonest = 0;

    for (i = 0; i < nholds; i++) {
        if (holds[i].code == code) {
            /* The terminal's auto-repeat: still held. */
            holds[i].until = now + HOLD_REPEAT;
            return;
        }
    }
    if (nholds == MAX_HOLDS) {
        for (i = 1; i < nholds; i++)
            if (holds[i].until < holds[soonest].until)
                soonest = i;
        to_engine(holds[soonest].code, 0, holds[soonest].mods);
        holds[soonest] = holds[--nholds];
    }
    holds[nholds].code = code;
    holds[nholds].mods = mods;
    holds[nholds].until = now + HOLD_FIRST;
    nholds++;
    to_engine(code, 1, mods);
}

void av_keys_expire(void)
{
    double now = av_now();
    int i = 0;

    while (i < nholds) {
        if (holds[i].until <= now) {
            to_engine(holds[i].code, 0, holds[i].mods);
            holds[i] = holds[--nholds];
        } else {
            i++;
        }
    }
}

/* ---- one decoded key ---- */

static int is_ctrl(unsigned mods, unsigned code, unsigned want)
{
    return (mods & RETROKMOD_CTRL) && code == want;
}

static void key_event(unsigned code, int down, unsigned mods)
{
    if (!code || code >= RETROK_LAST)
        return;
    if (sink.decoded)
        sink.decoded(code, down, mods);

    /* Frontend keys, from the decoded event so they work under the kitty
     * protocol too (there Ctrl-\ is CSI 92;5u, never byte 0x1C). */
    if (down && (code == RETROK_F10 || is_ctrl(mods, code, 'q') ||
                 is_ctrl(mods, code, '\\'))) {
        swallowed[code] = 1;
        av_term_request_quit();
        return;
    }
    if (down && is_ctrl(mods, code, 'g')) {
        swallowed[code] = 1;
        if (sink.release_toggle)
            sink.release_toggle();
        return;
    }
    if (swallowed[code]) {
        if (!down)
            swallowed[code] = 0;
        return;
    }

    if (kitty) {
        to_engine(code, down, mods);
        return;
    }
    if (!down)
        return;
    if (sink.text_input && sink.text_input()) {
        /* Typing: each press is a whole keystroke, so a letter typed
         * twice gives two letters. */
        to_engine(code, 1, mods);
        to_engine(code, 0, mods);
        return;
    }
    hold_press(code, mods);
}

/* Unicode code point (or kitty private code) to a RETROK code; 0 when
 * unknown. */
static unsigned code_point_key(unsigned cp, unsigned *mods)
{
    if (cp >= 'A' && cp <= 'Z') {
        *mods |= RETROKMOD_SHIFT;
        return cp - 'A' + 'a';
    }
    switch (cp) {
    case 27:  return RETROK_ESCAPE;
    case 13:  return RETROK_RETURN;
    case 9:   return RETROK_TAB;
    case 127: return RETROK_BACKSPACE;
    case 32:  return RETROK_SPACE;
    case 96:  return RETROK_BACKQUOTE;
    case 57441: return RETROK_LSHIFT;
    case 57442: return RETROK_LCTRL;
    case 57443: return RETROK_LALT;
    case 57447: return RETROK_RSHIFT;
    case 57448: return RETROK_RCTRL;
    case 57449: return RETROK_RALT;
    default: break;
    }
    /* Below 127 RETROK is ASCII. */
    if (cp > 0 && cp < 127)
        return cp;
    return 0;
}

/* Kitty's modifier field is 1-based: bit 0 shift, 1 alt, 2 ctrl, 3 meta. */
static unsigned kitty_mods(int field)
{
    unsigned m = 0, v = field > 0 ? (unsigned)(field - 1) : 0;

    if (v & 1) m |= RETROKMOD_SHIFT;
    if (v & 2) m |= RETROKMOD_ALT;
    if (v & 4) m |= RETROKMOD_CTRL;
    if (v & 8) m |= RETROKMOD_META;
    return m;
}

static unsigned final_key(unsigned char f)
{
    switch (f) {
    case 'A': return RETROK_UP;
    case 'B': return RETROK_DOWN;
    case 'C': return RETROK_RIGHT;
    case 'D': return RETROK_LEFT;
    case 'H': return RETROK_HOME;
    case 'F': return RETROK_END;
    case 'P': return RETROK_F1;
    case 'Q': return RETROK_F2;
    case 'R': return RETROK_F3;
    case 'S': return RETROK_F4;
    default:  return 0;
    }
}

static unsigned tilde_key(int n)
{
    switch (n) {
    case 1:  return RETROK_HOME;
    case 2:  return RETROK_INSERT;
    case 3:  return RETROK_DELETE;
    case 4:  return RETROK_END;
    case 5:  return RETROK_PAGEUP;
    case 6:  return RETROK_PAGEDOWN;
    /* rxvt's F1 to F4; kitty sends F3 as 13~ (CSI R is a cursor report). */
    case 11: return RETROK_F1;
    case 12: return RETROK_F2;
    case 13: return RETROK_F3;
    case 14: return RETROK_F4;
    case 15: return RETROK_F5;
    case 17: return RETROK_F6;
    case 18: return RETROK_F7;
    case 19: return RETROK_F8;
    case 20: return RETROK_F9;
    case 21: return RETROK_F10;
    case 23: return RETROK_F11;
    case 24: return RETROK_F12;
    default: return 0;
    }
}

/* ---- sequences ---- */

#define NPARAM 8
#define NSUB 4

/* Parses a CSI at p (p[0] ESC, p[1] '[').  Returns bytes consumed, 0 when
 * incomplete. */
static size_t parse_csi(const unsigned char *p, size_t n)
{
    int prm[NPARAM][NSUB], have[NPARAM][NSUB], np = 0, ns = 0;
    size_t i;
    unsigned char f;
    unsigned code, mods;
    int ev;

    memset(prm, 0, sizeof(prm));
    memset(have, 0, sizeof(have));
    for (i = 2; i < n; i++) {
        unsigned char c = p[i];

        if (c >= '0' && c <= '9') {
            if (np < NPARAM && ns < NSUB) {
                prm[np][ns] = prm[np][ns] * 10 + (c - '0');
                have[np][ns] = 1;
            }
        } else if (c == ';') {
            np++;
            ns = 0;
        } else if (c == ':') {
            ns++;
        } else if (c >= 0x3c && c <= 0x3f) {
            /* private markers: ? > < = */
        } else if (c >= 0x20 && c <= 0x2f) {
            /* intermediates */
        } else if (c >= 0x40 && c <= 0x7e) {
            break;
        } else {
            /* Not a CSI after all: drop it up to here. */
            return i;
        }
    }
    if (i >= n)
        return 0;
    f = p[i];
    i++;

    mods = kitty_mods(have[1][0] ? prm[1][0] : 1);
    ev = have[1][1] ? prm[1][1] : 1;
    if (ev == 2)
        return i;       /* repeat: the engine keeps its own */

    if (f == 'u') {
        unsigned extra = 0;

        code = code_point_key((unsigned)prm[0][0], &extra);
        key_event(code, ev != 3, mods);
    } else if (f == '~') {
        key_event(tilde_key(prm[0][0]), ev != 3, mods);
    } else if ((code = final_key(f)) != 0) {
        key_event(code, ev != 3, mods);
    }
    return i;
}

/* SGR pointer report at p: ESC [ < b ; x ; y M|m.  0 when incomplete. */
static size_t parse_sgr_mouse(const unsigned char *p, size_t n)
{
    long v[3] = { 0, 0, 0 };
    int field = 0, neg = 0, digits = 0;
    size_t i;

    for (i = 3; i < n; i++) {
        unsigned char c = p[i];

        if (c >= '0' && c <= '9') {
            v[field] = v[field] * 10 + (c - '0');
            if (v[field] > 1000000)
                v[field] = 1000000;
            digits = 1;
        } else if (c == '-' && !digits) {
            /* Coordinates go negative in a drag outside the window, and
             * read unsigned the view snaps across the map. */
            neg = 1;
        } else if (c == ';' && field < 2) {
            if (neg)
                v[field] = -v[field];
            field++;
            neg = digits = 0;
        } else if ((c == 'M' || c == 'm') && field == 2) {
            av_mouse_report r;
            int b = (int)v[0];

            if (neg)
                v[2] = -v[2];
            r.x = (int)v[1];
            r.y = (int)v[2];
            r.motion = (b & 32) != 0;
            r.release = c == 'm';
            /* 4, 8, 16 are modifiers. */
            if (b & 64)
                r.button = 3 + (b & 3);
            else if ((b & 3) == 3)
                r.button = -1;
            else
                r.button = b & 3;
            if (sink.mouse)
                sink.mouse(&r);
            return i + 1;
        } else if (c == 0x1b) {
            /* Malformed and a new sequence begins: keep that one. */
            return i;
        } else {
            return i + 1;
        }
    }
    return 0;
}

/* Decodes what it can; returns bytes consumed. */
static size_t parse(const unsigned char *p, size_t n, double now)
{
    size_t i = 0;

    while (i < n) {
        unsigned char c = p[i];
        size_t left = n - i, used;

        if (c != 0x1b) {
            unsigned mods = 0, code;

            esc_since = 0;
            i++;
            if (c == 0x1c && !kitty) {
                av_term_request_quit();
                continue;
            }
            if (c < 32 && c != 9 && c != 13) {
                /* Ctrl plus a letter: byte 1 is Ctrl-A. */
                mods = RETROKMOD_CTRL;
                if (c == 0)
                    code = RETROK_SPACE;
                else if (c <= 26)
                    code = 'a' + c - 1;
                else
                    code = (unsigned)c + 64;    /* ] ^ _ */
            } else if (c >= 128) {
                continue;       /* UTF-8 text: no key for it */
            } else {
                code = code_point_key(c, &mods);
            }
            key_event(code, 1, mods);
            continue;
        }

        if (left >= 3 && p[i + 1] == '[' && (p[i + 2] == 'I' ||
                                            p[i + 2] == 'O')) {
            if (sink.focus)
                sink.focus(p[i + 2] == 'I');
            i += 3;
            continue;
        }
        /* Before the generic CSI: that would take the report's M or m as
         * a key and its minus sign as a private marker. */
        if (left >= 3 && p[i + 1] == '[' && p[i + 2] == '<') {
            used = parse_sgr_mouse(p + i, left);
            if (!used)
                break;
            i += used;
            continue;
        }
        if (left >= 3 && p[i + 1] == '[' && p[i + 2] == 'M') {
            /* X10 report from a terminal without mode 1016: cell
             * resolution, capped at 223, no use for aiming. */
            if (left < 6)
                break;
            if (sink.x10)
                sink.x10();
            i += 6;
            continue;
        }
        if (left >= 2 && p[i + 1] == '[') {
            used = parse_csi(p + i, left);
            if (!used)
                break;
            i += used;
            continue;
        }
        if (left == 1) {
            /* Under the kitty protocol Escape itself is CSI 27 u, so a
             * lone ESC is almost always a sequence split across reads.
             * Taking it as Escape at once bounced the player out of
             * menus on nearly every key. */
            if (kitty) {
                if (!esc_since)
                    esc_since = now;
                if (now - esc_since < 0.050)
                    break;
            }
            esc_since = 0;
            key_event(RETROK_ESCAPE, 1, 0);
            if (kitty)
                key_event(RETROK_ESCAPE, 0, 0);
            i++;
            continue;
        }
        if (p[i + 1] == 'O') {
            unsigned code;

            /* SS3: arrows and F1 to F4 in application cursor mode. */
            if (left < 3)
                break;
            code = final_key(p[i + 2]);
            if (code)
                key_event(code, 1, 0);
            i += 3;
            continue;
        }
        /* ESC then a plain byte (Alt+key on a legacy terminal): Escape,
         * then the byte on the next pass. */
        key_event(RETROK_ESCAPE, 1, 0);
        if (kitty)
            key_event(RETROK_ESCAPE, 0, 0);
        i++;
    }
    return i;
}

void av_keys_init(const av_keys_sink *s, int kitty_on)
{
    sink = *s;
    kitty = kitty_on;
    len = 0;
    esc_since = 0;
    nholds = 0;
    memset(swallowed, 0, sizeof(swallowed));
}

void av_keys_poll(void)
{
    double now = av_now();

    for (;;) {
        int r = 0;
        size_t used;

        if (len < sizeof(buf)) {
            r = av_term_read((char *)buf + len, sizeof(buf) - len, 0);
            if (r > 0) {
                if (sink.raw)
                    sink.raw(buf + len, (size_t)r);
                len += (size_t)r;
            }
        }
        used = parse(buf, len, now);
        if (used) {
            memmove(buf, buf + used, len - used);
            len -= used;
        } else if (len == sizeof(buf)) {
            /* One absurd unterminated sequence filled the buffer: drop
             * it, or every key after it is stuck. */
            len = 0;
        }
        /* Keep reading until stdin is empty, or aiming falls behind. */
        if (r <= 0)
            break;
    }
}

const char *av_keys_name(unsigned code)
{
    static char b[16];

    switch (code) {
    case RETROK_BACKSPACE: return "Backspace";
    case RETROK_TAB:       return "Tab";
    case RETROK_RETURN:    return "Return";
    case RETROK_ESCAPE:    return "Escape";
    case RETROK_SPACE:     return "Space";
    case RETROK_DELETE:    return "Delete";
    case RETROK_UP:        return "Up";
    case RETROK_DOWN:      return "Down";
    case RETROK_LEFT:      return "Left";
    case RETROK_RIGHT:     return "Right";
    case RETROK_INSERT:    return "Insert";
    case RETROK_HOME:      return "Home";
    case RETROK_END:       return "End";
    case RETROK_PAGEUP:    return "PageUp";
    case RETROK_PAGEDOWN:  return "PageDown";
    case RETROK_LSHIFT:    return "LShift";
    case RETROK_RSHIFT:    return "RShift";
    case RETROK_LCTRL:     return "LCtrl";
    case RETROK_RCTRL:     return "RCtrl";
    case RETROK_LALT:      return "LAlt";
    case RETROK_RALT:      return "RAlt";
    default: break;
    }
    if (code >= RETROK_F1 && code <= RETROK_F12) {
        snprintf(b, sizeof(b), "F%u", code - RETROK_F1 + 1);
        return b;
    }
    if (code > 32 && code < 127) {
        snprintf(b, sizeof(b), "'%c'", (char)code);
        return b;
    }
    snprintf(b, sizeof(b), "#%u", code);
    return b;
}
