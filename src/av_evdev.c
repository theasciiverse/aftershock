/* Aftershock: a raw pointer from /dev/input.
 *
 * Why a raw device at all: a terminal reports the pointer as absolute
 * positions inside its window, so the view runs out of room at the window
 * edge.  A grabbed evdev node gives unbounded relative motion, as a
 * desktop game gets.
 *
 * The rules that keep this from reading a keyboard (see the spec):
 *  - nodes are classified by ioctl capability bitmaps only, and closed
 *    again; read() is only ever called on the chosen pointer;
 *  - a node that can type A, Z or Enter is a keyboard, whatever else it
 *    can do, and is final: also when named with --mouse-dev;
 *  - a pointer whose name is exactly a keyboard node's name is that
 *    keyboard's pointer interface (a Razer BlackWidow shows up as both;
 *    a mouse's own key node has a suffixed name instead).  No capability
 *    test tells those apart, the names do.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_evdev.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/input.h>
#include <sys/ioctl.h>
#endif

static int fd = -1, grabbed;
static char cur_path[64], cur_name[128];

const char *av_evdev_verdict(int verdict)
{
    switch (verdict) {
    case AV_EV_POINTER:     return "pointer";
    case AV_EV_KEYBOARD:    return "keyboard - never opened";
    case AV_EV_KBD_POINTER: return "keyboard's pointer - rejected";
    case AV_EV_NOT_POINTER: return "not a pointer";
    default:                return "unreadable";
    }
}

int av_evdev_is_open(void)
{
    return fd >= 0;
}

const char *av_evdev_path(void)
{
    return cur_path;
}

const char *av_evdev_name(void)
{
    return cur_name;
}

#ifdef __linux__

#define NBITS(n) (((n) + 8 * sizeof(unsigned long) - 1) / \
                  (8 * sizeof(unsigned long)))
#define TEST(bit, arr) (((arr)[(bit) / (8 * sizeof(unsigned long))] >> \
                         ((bit) % (8 * sizeof(unsigned long)))) & 1UL)

/* Capability verdict from ioctls alone (no keyboard-name rule yet). */
static int classify(const char *path, char *name, size_t namecap,
                    int *score)
{
    unsigned long ev[NBITS(EV_MAX + 1)], keys[NBITS(KEY_MAX + 1)],
                  rel[NBITS(REL_MAX + 1)];
    int f, v;

    *score = 0;
    if (namecap)
        name[0] = 0;
    f = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (f < 0)
        return AV_EV_UNREADABLE;
    memset(ev, 0, sizeof(ev));
    memset(keys, 0, sizeof(keys));
    memset(rel, 0, sizeof(rel));
    if (namecap && ioctl(f, EVIOCGNAME(namecap - 1), name) < 0)
        name[0] = 0;
    if (namecap)
        name[namecap - 1] = 0;
    if (ioctl(f, EVIOCGBIT(0, sizeof(ev)), ev) < 0) {
        close(f);
        return AV_EV_UNREADABLE;
    }
    if (TEST(EV_KEY, ev))
        ioctl(f, EVIOCGBIT(EV_KEY, sizeof(keys)), keys);
    if (TEST(EV_REL, ev))
        ioctl(f, EVIOCGBIT(EV_REL, sizeof(rel)), rel);
    close(f);

    if (TEST(EV_KEY, ev) && (TEST(KEY_A, keys) || TEST(KEY_Z, keys) ||
                             TEST(KEY_ENTER, keys)))
        return AV_EV_KEYBOARD;
    if (!TEST(EV_REL, ev) || !TEST(REL_X, rel) || !TEST(REL_Y, rel) ||
        !TEST(EV_KEY, ev) || !TEST(BTN_LEFT, keys))
        return AV_EV_NOT_POINTER;

    v = 0;
    {
        char low[128];
        size_t i;

        for (i = 0; i + 1 < sizeof(low) && name[i]; i++)
            low[i] = (char)tolower((unsigned char)name[i]);
        low[i] = 0;
        if (strstr(low, "mouse") || strstr(low, "trackball"))
            v += 100;
    }
    if (TEST(REL_HWHEEL, rel))
        v += 10;
    *score = v;
    return AV_EV_POINTER;
}

static int cmp_event(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

int av_evdev_scan(av_evdev_node *out, int max)
{
    DIR *d = opendir("/dev/input");
    struct dirent *e;
    int nums[512], n = 0, i, j, count;

    if (!d)
        return 0;
    while ((e = readdir(d)) != NULL && n < 512) {
        char *end;
        long v;

        if (strncmp(e->d_name, "event", 5) != 0 || !e->d_name[5])
            continue;
        v = strtol(e->d_name + 5, &end, 10);
        if (*end || v < 0)
            continue;
        nums[n++] = (int)v;
    }
    closedir(d);
    /* event2 before event10. */
    qsort(nums, (size_t)n, sizeof(int), cmp_event);

    count = n < max ? n : max;
    for (i = 0; i < count; i++) {
        snprintf(out[i].path, sizeof(out[i].path), "/dev/input/event%d",
                 nums[i]);
        out[i].verdict = classify(out[i].path, out[i].name,
                                  sizeof(out[i].name), &out[i].score);
    }
    /* Keyboard names first (up to 32), then a pointer carrying one of
     * them is that keyboard's pointer. */
    {
        const char *kbd[32];
        int nk = 0;

        for (i = 0; i < count && nk < 32; i++)
            if (out[i].verdict == AV_EV_KEYBOARD && out[i].name[0])
                kbd[nk++] = out[i].name;
        for (i = 0; i < count; i++) {
            if (out[i].verdict != AV_EV_POINTER)
                continue;
            for (j = 0; j < nk; j++)
                if (strcmp(out[i].name, kbd[j]) == 0)
                    out[i].verdict = AV_EV_KBD_POINTER;
        }
    }
    return count;
}

void av_evdev_list(FILE *f)
{
    av_evdev_node nodes[256];
    int n = av_evdev_scan(nodes, 256), i;

    if (n == 0)
        fprintf(f, "no /dev/input/event* nodes\n");
    for (i = 0; i < n; i++)
        fprintf(f, "%-20s %-30s %s\n", nodes[i].path,
                av_evdev_verdict(nodes[i].verdict),
                nodes[i].name[0] ? nodes[i].name : "(no name)");
}

int av_evdev_open(const char *path, char *err, size_t errcap)
{
    av_evdev_node nodes[256];
    int n, i, pick = -1;

    av_evdev_close();
    n = av_evdev_scan(nodes, 256);
    if (path) {
        for (i = 0; i < n; i++)
            if (strcmp(nodes[i].path, path) == 0)
                pick = i;
        if (pick < 0) {
            /* Not under /dev/input/event*: classify it alone, the
             * keyboard-name rule still applies against the real nodes. */
            if (n >= 256)
                n = 255;
            snprintf(nodes[n].path, sizeof(nodes[n].path), "%s", path);
            nodes[n].verdict = classify(path, nodes[n].name,
                                        sizeof(nodes[n].name),
                                        &nodes[n].score);
            for (i = 0; i < n && nodes[n].verdict == AV_EV_POINTER; i++)
                if (nodes[i].verdict == AV_EV_KEYBOARD &&
                    nodes[i].name[0] &&
                    strcmp(nodes[i].name, nodes[n].name) == 0)
                    nodes[n].verdict = AV_EV_KBD_POINTER;
            pick = n;
        }
        if (nodes[pick].verdict != AV_EV_POINTER) {
            snprintf(err, errcap, "%s: %s", path,
                     av_evdev_verdict(nodes[pick].verdict));
            return -1;
        }
    } else {
        int unreadable = 0;

        for (i = 0; i < n; i++) {
            if (nodes[i].verdict == AV_EV_UNREADABLE)
                unreadable++;
            if (nodes[i].verdict != AV_EV_POINTER)
                continue;
            /* Strictly greater: ties go to the lower number. */
            if (pick < 0 || nodes[i].score > nodes[pick].score)
                pick = i;
        }
        if (pick < 0) {
            if (unreadable)
                snprintf(err, errcap, "no readable pointer in /dev/input "
                         "(%d nodes unreadable; the input group can read "
                         "them)", unreadable);
            else
                snprintf(err, errcap, "no pointer in /dev/input");
            return -1;
        }
    }
    fd = open(nodes[pick].path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        snprintf(err, errcap, "%s: %s", nodes[pick].path, strerror(errno));
        return -1;
    }
    snprintf(cur_path, sizeof(cur_path), "%s", nodes[pick].path);
    snprintf(cur_name, sizeof(cur_name), "%s", nodes[pick].name);
    return 0;
}

int av_evdev_grab(int on)
{
    if (fd < 0)
        return -1;
    if (!!on == grabbed)
        return 0;
    /* While grabbed the compositor no longer sees the device: the desktop
     * pointer freezes.  The kernel drops the grab when fd closes, so even
     * a crash gives the mouse back. */
    if (ioctl(fd, EVIOCGRAB, on ? 1 : 0) != 0)
        return -1;
    grabbed = !!on;
    return 0;
}

int av_evdev_drain(int *dx, int *dy, void (*fn)(int button, int down))
{
    struct input_event ev[64];
    int reads;

    if (fd < 0)
        return 0;
    for (reads = 0; reads < 32; reads++) {
        ssize_t r = read(fd, ev, sizeof(ev));
        size_t i, n;

        if (r < 0) {
            if (errno == EAGAIN || errno == EINTR)
                return 0;
            /* Unplugged: play on without it. */
            av_evdev_close();
            return -1;
        }
        if (r == 0)
            return 0;
        n = (size_t)r / sizeof(ev[0]);
        for (i = 0; i < n; i++) {
            const struct input_event *e = &ev[i];

            if (e->type == EV_REL) {
                if (e->code == REL_X)
                    *dx += e->value;
                else if (e->code == REL_Y)
                    *dy += e->value;
                else if (e->code == REL_WHEEL && e->value && fn)
                    fn(e->value > 0 ? 3 : 4, 1);
                else if (e->code == REL_HWHEEL && e->value && fn)
                    fn(e->value > 0 ? 6 : 5, 1);
            } else if (e->type == EV_KEY && e->value != 2 && fn) {
                if (e->code == BTN_LEFT)
                    fn(0, e->value);
                else if (e->code == BTN_MIDDLE)
                    fn(1, e->value);
                else if (e->code == BTN_RIGHT)
                    fn(2, e->value);
            }
        }
        if ((size_t)r < sizeof(ev))
            return 0;
    }
    return 0;
}

void av_evdev_close(void)
{
    if (fd < 0)
        return;
    if (grabbed)
        ioctl(fd, EVIOCGRAB, 0);
    grabbed = 0;
    close(fd);
    fd = -1;
}

#else /* not Linux: no evdev, auto lands on the terminal */

int av_evdev_scan(av_evdev_node *out, int max)
{
    (void)out;
    (void)max;
    return 0;
}

void av_evdev_list(FILE *f)
{
    fprintf(f, "evdev pointers are Linux only\n");
}

int av_evdev_open(const char *path, char *err, size_t errcap)
{
    (void)path;
    snprintf(err, errcap, "evdev pointers are Linux only");
    return -1;
}

int av_evdev_grab(int on)
{
    (void)on;
    return -1;
}

int av_evdev_drain(int *dx, int *dy, void (*fn)(int button, int down))
{
    (void)dx;
    (void)dy;
    (void)fn;
    return 0;
}

void av_evdev_close(void)
{
}

#endif
