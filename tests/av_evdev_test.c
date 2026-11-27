/* Aftershock: tests for the evdev pointer's device selection.
 *
 * Reading the wrong event device is keylogging, so this checks the rules
 * of src/av_evdev.c against the real machine and against synthetic devices
 * made through /dev/uinput:
 *  - every keyboard-capable node on the machine is classified a keyboard;
 *  - a pointer is accepted, and its motion, wheel and buttons arrive with
 *    the right numbering;
 *  - a keyboard is refused, also when named explicitly;
 *  - a device with both keys and pointer axes is refused, and
 *    auto-detection never lands on it;
 *  - a pointer named exactly like a keyboard is that keyboard's pointer.
 * The uinput part is skipped, with a note, where it is not permitted.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_evdev.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#endif

static int failures, checks;

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL %s\n", what);
    } else {
        printf("ok   %s\n", what);
    }
}

#ifdef __linux__

#define NBITS(n) (((n) + 8 * sizeof(unsigned long) - 1) / \
                  (8 * sizeof(unsigned long)))
#define TEST(bit, arr) (((arr)[(bit) / (8 * sizeof(unsigned long))] >> \
                         ((bit) % (8 * sizeof(unsigned long)))) & 1UL)

/* Our own reading of the bitmaps, independent of av_evdev.c's: 1 when the
 * node can type a letter or Enter, -1 when it cannot be opened. */
static int can_type(const char *path)
{
    unsigned long ev[NBITS(EV_MAX + 1)], keys[NBITS(KEY_MAX + 1)];
    int f = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC), r = 0;

    if (f < 0)
        return -1;
    memset(ev, 0, sizeof(ev));
    memset(keys, 0, sizeof(keys));
    if (ioctl(f, EVIOCGBIT(0, sizeof(ev)), ev) >= 0 && TEST(EV_KEY, ev) &&
        ioctl(f, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0) {
        r = TEST(KEY_A, keys) || TEST(KEY_Z, keys) || TEST(KEY_ENTER, keys);
    }
    close(f);
    return r;
}

static void test_machine(void)
{
    av_evdev_node nodes[256];
    int n = av_evdev_scan(nodes, 256), i, seen = 0, unreadable = 0;

    for (i = 0; i < n; i++) {
        int t = can_type(nodes[i].path);
        char what[256];

        if (t < 0) {
            unreadable++;
            continue;
        }
        if (!t)
            continue;
        seen++;
        snprintf(what, sizeof(what), "%.63s (%.127s) can type: classified a "
                 "keyboard", nodes[i].path, nodes[i].name);
        check(nodes[i].verdict == AV_EV_KEYBOARD, what);
    }
    if (unreadable == n)
        printf("note: none of the %d /dev/input nodes is readable here "
               "(not in the input group); machine check skipped\n", n);
    else
        printf("note: %d keyboard-capable nodes checked, %d unreadable\n",
               seen, unreadable);
}

/* ---- synthetic devices ---- */

typedef struct {
    int fd;
    char path[64];
} udev_t;

static int set_bits(int fd, unsigned long req, const int *bits)
{
    for (; *bits >= 0; bits++)
        if (ioctl(fd, req, *bits) < 0)
            return -1;
    return 0;
}

static void nap(int ms)
{
    struct timespec ts = { 0, ms * 1000000L };

    nanosleep(&ts, NULL);
}

/* Makes a device and finds its node by name (the last match, as the
 * newest gets the highest number).  -1 when uinput is not permitted. */
static int make_dev(udev_t *d, const char *name, const int *keys,
                    const int *rels)
{
    struct uinput_setup us;
    av_evdev_node nodes[256];
    int n, i, tries;

    d->fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    d->path[0] = 0;
    if (d->fd < 0)
        return -1;
    if (keys) {
        ioctl(d->fd, UI_SET_EVBIT, EV_KEY);
        set_bits(d->fd, UI_SET_KEYBIT, keys);
    }
    if (rels) {
        ioctl(d->fd, UI_SET_EVBIT, EV_REL);
        set_bits(d->fd, UI_SET_RELBIT, rels);
    }
    memset(&us, 0, sizeof(us));
    us.id.bustype = BUS_VIRTUAL;
    us.id.vendor = 0x1234;
    us.id.product = 0x5678;
    snprintf(us.name, sizeof(us.name), "%s", name);
    if (ioctl(d->fd, UI_DEV_SETUP, &us) < 0 ||
        ioctl(d->fd, UI_DEV_CREATE) < 0) {
        close(d->fd);
        d->fd = -1;
        return -1;
    }
    /* udev needs a moment to make the node and set its permissions. */
    for (tries = 0; tries < 40 && !d->path[0]; tries++) {
        nap(50);
        n = av_evdev_scan(nodes, 256);
        for (i = 0; i < n; i++)
            if (strcmp(nodes[i].name, name) == 0)
                memcpy(d->path, nodes[i].path, sizeof(d->path));
    }
    return 0;
}

static void drop_dev(udev_t *d)
{
    if (d->fd >= 0) {
        ioctl(d->fd, UI_DEV_DESTROY);
        close(d->fd);
    }
    d->fd = -1;
}

static void emit(int fd, int type, int code, int value)
{
    struct input_event e;

    memset(&e, 0, sizeof(e));
    e.type = (unsigned short)type;
    e.code = (unsigned short)code;
    e.value = value;
    if (write(fd, &e, sizeof(e)) < 0)
        perror("uinput write");
}

static int verdict_of(const char *path)
{
    av_evdev_node nodes[256];
    int n = av_evdev_scan(nodes, 256), i;

    for (i = 0; i < n; i++)
        if (strcmp(nodes[i].path, path) == 0)
            return nodes[i].verdict;
    return -1;
}

static int got[16][2], ngot;

static void on_button(int b, int down)
{
    if (ngot < 16) {
        got[ngot][0] = b;
        got[ngot][1] = down;
        ngot++;
    }
}

static const int mouse_keys[] = { BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, -1 };
static const int mouse_rels[] = { REL_X, REL_Y, REL_WHEEL, REL_HWHEEL, -1 };
static const int kbd_keys[] = { KEY_A, KEY_Z, KEY_ENTER, KEY_Q, KEY_W, -1 };
static const int combo_keys[] = { KEY_A, KEY_ENTER, BTN_LEFT, BTN_RIGHT, -1 };

static void test_synthetic(void)
{
    udev_t mouse, kbd, combo, twin_k, twin_p;
    char err[256], what[256];
    int dx = 0, dy = 0, tries;

    if (make_dev(&mouse, "av test pointer mouse", mouse_keys,
                 mouse_rels) != 0) {
        printf("note: /dev/uinput not permitted (%s); synthetic device "
               "checks skipped\n", strerror(errno));
        return;
    }
    make_dev(&kbd, "av test keyboard", kbd_keys, NULL);
    make_dev(&combo, "av test combo", combo_keys, mouse_rels);
    make_dev(&twin_k, "av test twin", kbd_keys, NULL);
    make_dev(&twin_p, "av test twin", mouse_keys, mouse_rels);
    /* Both twins share a name: find the pointer one by exclusion. */
    {
        av_evdev_node nodes[256];
        int n = av_evdev_scan(nodes, 256), i;

        twin_k.path[0] = twin_p.path[0] = 0;
        for (i = 0; i < n; i++) {
            if (strcmp(nodes[i].name, "av test twin") != 0)
                continue;
            if (nodes[i].verdict == AV_EV_KEYBOARD)
                memcpy(twin_k.path, nodes[i].path, sizeof(twin_k.path));
            else
                memcpy(twin_p.path, nodes[i].path, sizeof(twin_p.path));
        }
    }

    if (!mouse.path[0] || verdict_of(mouse.path) == AV_EV_UNREADABLE) {
        printf("note: synthetic nodes are not readable by this user (udev "
               "gives them to the input group); synthetic checks "
               "skipped\n");
        goto out;
    }

    check(verdict_of(mouse.path) == AV_EV_POINTER, "synthetic mouse is a "
          "pointer");
    check(kbd.path[0] && verdict_of(kbd.path) == AV_EV_KEYBOARD,
          "synthetic keyboard is a keyboard");
    check(combo.path[0] && verdict_of(combo.path) == AV_EV_KEYBOARD,
          "keys plus pointer axes is a keyboard");
    check(twin_p.path[0] && verdict_of(twin_p.path) == AV_EV_KBD_POINTER,
          "pointer named like a keyboard is that keyboard's pointer");

    check(av_evdev_open(kbd.path, err, sizeof(err)) != 0,
          "keyboard refused when named explicitly");
    check(av_evdev_open(combo.path, err, sizeof(err)) != 0,
          "keys-plus-axes refused when named explicitly");
    check(av_evdev_open(twin_p.path, err, sizeof(err)) != 0,
          "keyboard's pointer refused when named explicitly");

    if (av_evdev_open(NULL, err, sizeof(err)) == 0) {
        snprintf(what, sizeof(what), "auto-detection (%s) never lands on "
                 "a keyboard-capable node", av_evdev_path());
        check(strcmp(av_evdev_path(), combo.path) != 0 &&
              strcmp(av_evdev_path(), kbd.path) != 0 &&
              strcmp(av_evdev_path(), twin_p.path) != 0, what);
        av_evdev_close();
    }

    check(av_evdev_open(mouse.path, err, sizeof(err)) == 0,
          "synthetic mouse opens when named");
    if (av_evdev_is_open()) {
        nap(100);
        av_evdev_drain(&dx, &dy, NULL);     /* anything from creation */
        dx = dy = 0;
        ngot = 0;
        emit(mouse.fd, EV_REL, REL_X, 7);
        emit(mouse.fd, EV_REL, REL_Y, -3);
        emit(mouse.fd, EV_REL, REL_WHEEL, 1);
        emit(mouse.fd, EV_REL, REL_WHEEL, -1);
        emit(mouse.fd, EV_REL, REL_HWHEEL, -1);
        emit(mouse.fd, EV_REL, REL_HWHEEL, 1);
        emit(mouse.fd, EV_KEY, BTN_LEFT, 1);
        emit(mouse.fd, EV_KEY, BTN_MIDDLE, 1);
        emit(mouse.fd, EV_KEY, BTN_RIGHT, 1);
        emit(mouse.fd, EV_KEY, BTN_RIGHT, 0);
        emit(mouse.fd, EV_SYN, SYN_REPORT, 0);
        for (tries = 0; tries < 20 && ngot < 8; tries++) {
            nap(20);
            av_evdev_drain(&dx, &dy, on_button);
        }
        check(dx == 7 && dy == -3, "motion arrives");
        check(ngot == 8 &&
              got[0][0] == 3 && got[1][0] == 4 &&   /* wheel up, down */
              got[2][0] == 5 && got[3][0] == 6 &&   /* left, right */
              got[4][0] == 0 && got[4][1] == 1 &&   /* left down */
              got[5][0] == 1 && got[6][0] == 2 &&   /* middle, right */
              got[7][0] == 2 && got[7][1] == 0,     /* right up */
              "wheel and buttons arrive with the right numbering");
        check(av_evdev_grab(1) == 0 && av_evdev_grab(0) == 0,
              "grab and ungrab");
        av_evdev_close();
    }
out:
    drop_dev(&twin_p);
    drop_dev(&twin_k);
    drop_dev(&combo);
    drop_dev(&kbd);
    drop_dev(&mouse);
}

#endif

int main(void)
{
#ifdef __linux__
    test_machine();
    test_synthetic();
#else
    printf("note: evdev is Linux only; nothing to test\n");
#endif
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
