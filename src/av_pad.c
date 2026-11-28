/* Aftershock: a gamepad, read from Linux's /dev/input.
 *
 * The first device that looks like a gamepad (sticks plus the south face
 * button) is opened read-only and never grabbed.  Desktop sessions usually
 * let the logged-in user read gamepads; when none can be opened the game
 * simply plays without one.
 *
 * The engine's port is a keyboard (the mouse path needs that), so the pad is
 * turned into what the keyboard and mouse already speak:
 *   left stick    walk and step sideways (AUX keys, bound in default.cfg)
 *   right stick   look, as mouse motion
 *   d-pad         the arrow keys
 *   buttons       the engine's JOY_ keys, bound in default.cfg
 * In menus and the console, A is Enter and B is Escape, so the pad can find
 * its way around them.
 */
#include "av_pad.h"
#include "av_host.h"

#include <libretro.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef __linux__
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>

int aftershock_text_input(void);

/* In the engine (libretro.c): a key's number from its name, -1 if none. */
int aftershock_keynum(const char *name);

static int fd = -1;
static char devname[128];
static int axis_min[ABS_CNT], axis_max[ABS_CNT];
static int axis[ABS_CNT];
static int stick_held[4];       /* forward, back, left, right */
static int key_aux[4];
static int hat_x, hat_y;

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define TEST_BIT(b, a) (((a)[(b) / BITS_PER_LONG] >> ((b) % BITS_PER_LONG)) & 1)

static int looks_like_pad(int f)
{
    unsigned long ev[NBITS(EV_MAX)], abs[NBITS(ABS_MAX)], key[NBITS(KEY_MAX)];

    memset(ev, 0, sizeof(ev));
    memset(abs, 0, sizeof(abs));
    memset(key, 0, sizeof(key));
    if (ioctl(f, EVIOCGBIT(0, sizeof(ev)), ev) < 0 ||
        !TEST_BIT(EV_ABS, ev) || !TEST_BIT(EV_KEY, ev))
        return 0;
    ioctl(f, EVIOCGBIT(EV_ABS, sizeof(abs)), abs);
    ioctl(f, EVIOCGBIT(EV_KEY, sizeof(key)), key);
    /* A gamepad: two axes and the south face button.  Keyboards and mice
     * have no BTN_SOUTH, so a keyboard is never read here. */
    return TEST_BIT(ABS_X, abs) && TEST_BIT(ABS_Y, abs) &&
           TEST_BIT(BTN_SOUTH, key) && !TEST_BIT(KEY_A, key);
}

int av_pad_open(void)
{
    DIR *d = opendir("/dev/input");
    struct dirent *e;
    int i;

    if (!d)
        return -1;
    while ((e = readdir(d)) != NULL) {
        char path[300];
        int f;

        if (strncmp(e->d_name, "event", 5))
            continue;
        snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
        f = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (f < 0)
            continue;
        if (looks_like_pad(f)) {
            fd = f;
            ioctl(f, EVIOCGNAME(sizeof(devname)), devname);
            break;
        }
        close(f);
    }
    closedir(d);
    if (fd < 0)
        return -1;
    for (i = 0; i < ABS_CNT; i++) {
        struct input_absinfo ai;

        axis_min[i] = -32768;
        axis_max[i] = 32767;
        if (ioctl(fd, EVIOCGABS(i), &ai) == 0 && ai.maximum > ai.minimum) {
            axis_min[i] = ai.minimum;
            axis_max[i] = ai.maximum;
            axis[i] = ai.value;
        }
    }
    key_aux[0] = aftershock_keynum("AUX4");     /* forward */
    key_aux[1] = aftershock_keynum("AUX3");     /* back */
    key_aux[2] = aftershock_keynum("AUX2");     /* left */
    key_aux[3] = aftershock_keynum("AUX1");     /* right */
    av_host_log("pad: %s\n", devname);
    return 0;
}

const char *av_pad_name(void)
{
    return fd >= 0 ? devname : NULL;
}

/* -1..1 with a dead zone. */
static double norm(int code)
{
    double mid = (axis_min[code] + axis_max[code]) / 2.0;
    double half = (axis_max[code] - axis_min[code]) / 2.0;
    double v = half > 0 ? (axis[code] - mid) / half : 0;

    if (v > -0.22 && v < 0.22)
        return 0;
    return v;
}

static void key(int keynum, int down)
{
    if (keynum > 0)
        av_host_key((unsigned)keynum, down, 0);
}

static void button(int code, int down)
{
    const char *name = NULL;
    int typing = aftershock_text_input();

    /* Menus and the console: A chooses, B goes back. */
    if (typing && down && (code == BTN_SOUTH || code == BTN_EAST)) {
        key(code == BTN_SOUTH ? RETROK_RETURN : RETROK_ESCAPE, 1);
        key(code == BTN_SOUTH ? RETROK_RETURN : RETROK_ESCAPE, 0);
        return;
    }
    switch (code) {
    case BTN_SOUTH:  name = "JOY_A"; break;
    case BTN_EAST:   name = "JOY_B"; break;
    case BTN_NORTH:  name = "JOY_Y"; break;
    case BTN_WEST:   name = "JOY_X"; break;
    case BTN_TL:     name = "JOY_L"; break;
    case BTN_TR:     name = "JOY_R"; break;
    case BTN_TL2:    name = "JOY_L2"; break;
    case BTN_TR2:    name = "JOY_R2"; break;
    case BTN_THUMBL: name = "JOY_L3"; break;
    case BTN_THUMBR: name = "JOY_R3"; break;
    case BTN_START:  name = "JOY_START"; break;
    case BTN_SELECT: name = "JOY_SELECT"; break;
    case BTN_DPAD_UP:    key(RETROK_UP, down); return;
    case BTN_DPAD_DOWN:  key(RETROK_DOWN, down); return;
    case BTN_DPAD_LEFT:  key(RETROK_LEFT, down); return;
    case BTN_DPAD_RIGHT: key(RETROK_RIGHT, down); return;
    }
    if (name)
        key(aftershock_keynum(name), down);
}

/* Analogue triggers on pads that have no trigger buttons. */
static int trig_l, trig_r;

static void hat(int code, int v)
{
    if (code == ABS_HAT0X) {
        if (hat_x != v) {
            if (hat_x < 0) key(RETROK_LEFT, 0);
            if (hat_x > 0) key(RETROK_RIGHT, 0);
            if (v < 0) key(RETROK_LEFT, 1);
            if (v > 0) key(RETROK_RIGHT, 1);
            hat_x = v;
        }
    } else {
        if (hat_y != v) {
            if (hat_y < 0) key(RETROK_UP, 0);
            if (hat_y > 0) key(RETROK_DOWN, 0);
            if (v < 0) key(RETROK_UP, 1);
            if (v > 0) key(RETROK_DOWN, 1);
            hat_y = v;
        }
    }
}

void av_pad_poll(int focused)
{
    struct input_event ev[64];
    ssize_t n;
    int i, want[4];
    double lx, ly, rx, ry;

    if (fd < 0)
        return;
    for (;;) {
        n = read(fd, ev, sizeof(ev));
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                av_host_log("pad: gone\n");
                close(fd);
                fd = -1;
                return;
            }
            break;
        }
        for (i = 0; i < (int)(n / (ssize_t)sizeof(ev[0])); i++) {
            if (!focused)
                continue;
            if (ev[i].type == EV_KEY) {
                button(ev[i].code, ev[i].value != 0);
            } else if (ev[i].type == EV_ABS && ev[i].code < ABS_CNT) {
                axis[ev[i].code] = ev[i].value;
                if (ev[i].code == ABS_HAT0X || ev[i].code == ABS_HAT0Y)
                    hat(ev[i].code, ev[i].value);
                if (ev[i].code == ABS_Z || ev[i].code == ABS_RZ) {
                    int *t = ev[i].code == ABS_Z ? &trig_l : &trig_r;
                    int down = (axis[ev[i].code] - axis_min[ev[i].code]) >
                               (axis_max[ev[i].code] - axis_min[ev[i].code]) / 2;

                    if (down != *t) {
                        *t = down;
                        key(aftershock_keynum(ev[i].code == ABS_Z ? "JOY_L2"
                                                                  : "JOY_R2"),
                            down);
                    }
                }
            }
        }
    }
    if (!focused)
        return;

    /* Left stick: walk and step, as held keys. */
    lx = norm(ABS_X);
    ly = norm(ABS_Y);
    want[0] = ly < -0.3;
    want[1] = ly > 0.3;
    want[2] = lx < -0.3;
    want[3] = lx > 0.3;
    for (i = 0; i < 4; i++) {
        if (want[i] != stick_held[i]) {
            stick_held[i] = want[i];
            key(key_aux[i], want[i]);
        }
    }

    /* Right stick: look, as mouse motion, faster towards the edge. */
    rx = norm(ABS_RX);
    ry = norm(ABS_RY);
    if (rx || ry)
        av_host_mouse_move((int)(rx * fabs(rx) * 9.0),
                            (int)(ry * fabs(ry) * 5.0));
}

void av_pad_close(void)
{
    if (fd >= 0)
        close(fd);
    fd = -1;
}

#else

int av_pad_open(void) { return -1; }
const char *av_pad_name(void) { return NULL; }
void av_pad_poll(int focused) { (void)focused; }
void av_pad_close(void) { }

#endif
