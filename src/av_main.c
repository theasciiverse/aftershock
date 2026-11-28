/* Aftershock: the command line, the start-up order and the game loop,
 * plus the two diagnostic modes (--demo, --keytest).
 *
 * Threads: this file's loop is the engine thread.  Once the render thread
 * runs (av_render.c) it is the only writer to the terminal, so nothing
 * here writes to stdout during the game; diagnostics go to the --log file,
 * or to stderr after the terminal is restored.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_clock.h"
#include "av_data.h"
#include "av_evdev.h"
#include "av_host.h"
#include "av_keys.h"
#include "av_overlay.h"
#include "av_pad.h"
#include "av_pictures.h"
#include "av_pointer.h"
#include "av_render.h"
#include "av_term.h"

#define AV_TEXTUI_FRONTEND
#include "av_textui.h"

#include <libretro.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

/* The engine's hook (third_party/tyrquake/common/libretro.c). */
int aftershock_text_input(void);

/* ---- options ---- */

static struct {
    const char *video;
    int symbols, color;
    int demo, find_pak, keytest;
    long frames;
    int frames_given;
    int cell_w, cell_h;
    const char *res;
    int fps;
    int cap_cols, cap_rows;
    const char *log;
    int sound, pad;
    av_pointer_opts mouse;
    int mouse_list;
    const char *data;
} opt;

static const char *prog = "aftershock";

static void usage(FILE *f)
{
    fprintf(f,
"usage: %s [options] [data folder holding aftershock/ and id1/]\n"
"\n"
"  --video=NAME      picture: %s\n"
"                    (default: the Picture chosen in Options, Video; else half)\n"
"  --symbols=SET     ascii | block | fine, for chafa and caca (default fine)\n"
"  --color=DEPTH     mono | 16 | 256 | true (default true)\n"
"  --demo            a test pattern instead of the game\n"
"  --find-pak        print the game data a normal run would load, and exit\n"
"  --keytest         show what every key and pointer report decodes to\n"
"  --frames=N        stop after N frames, 0 for no limit (--demo: 600)\n"
"  --cell=WxH        cell size in pixels when the terminal does not say\n"
"                    (default 10x20)\n"
"  --res=WxH|auto    engine resolution (default auto, to fit the terminal)\n"
"  --fps=N           pictures a second at most, 0 for every frame (default 30)\n"
"  --cells=WxH       largest picture in cells, 0x0 fills the terminal\n"
"  --log=PATH        session log: console, keys, player state\n"
"  --sound           sound and music (the default)\n"
"  --no-sound        silence: the audio device is never opened\n"
"  --no-pad          do not open a gamepad\n"
"  --mouse=SRC       auto | evdev | term | none (default auto: a raw\n"
"                    pointer from /dev/input if one can be read, else the\n"
"                    terminal's own reports)\n"
"  --mouse-dev=PATH  this /dev/input node instead of searching\n"
"  --mouse-list      list /dev/input/event* with a verdict each, and exit\n"
"  --no-mouse        same as --mouse=none\n"
"  --mouse-sens=F    engine counts per pointer pixel (default 2.0)\n"
"  --mouse-edge=F    terminal pointer: steering band at each side, as a\n"
"                    fraction; 0 turns it off (default 0.15)\n"
"  --mouse-turn=F    terminal pointer: turn rate at the very edge, deg/s\n"
"                    (default 220)\n"
"  --mouse-invert    invert pitch\n"
"  --help            this text\n"
"\n"
"Keys of our own: F10, Ctrl-Q or Ctrl-\\ quit; Ctrl-G lets the pointer go\n"
"and takes it back.\n", prog, av_backend_names());
}

static void bad_usage(void)
{
    usage(stderr);
    exit(2);
}

static int parse_wxh(const char *s, int *w, int *h)
{
    char *end;
    long a, b;

    a = strtol(s, &end, 10);
    if (end == s || (*end != 'x' && *end != 'X'))
        return -1;
    s = end + 1;
    b = strtol(s, &end, 10);
    if (end == s || *end || a < 0 || b < 0 || a > 100000 || b > 100000)
        return -1;
    *w = (int)a;
    *h = (int)b;
    return 0;
}

static double parse_float(const char *s)
{
    char *end;
    double v = strtod(s, &end);

    if (end == s || *end)
        bad_usage();
    return v;
}

static long parse_long(const char *s)
{
    char *end;
    long v = strtol(s, &end, 10);

    if (end == s || *end || v < 0)
        bad_usage();
    return v;
}

/* The value of --name=value, or NULL when arg is not that option. */
static const char *value_of(const char *arg, const char *name)
{
    size_t n = strlen(name);

    if (strncmp(arg, name, n) == 0 && arg[n] == '=')
        return arg + n + 1;
    return NULL;
}

static void parse_args(int argc, char **argv)
{
    int i;
    const char *v;

    opt.symbols = AV_SYM_FINE;
    opt.color = AV_COLOR_TRUE;
    opt.cell_w = 10;
    opt.cell_h = 20;
    opt.res = "auto";
    opt.fps = 30;
    opt.sound = 1;
    opt.pad = 1;
    opt.mouse.mode = AV_MOUSE_AUTO;
    opt.mouse.look.sens = 2.0;
    opt.mouse.look.edge = 0.15;
    opt.mouse.look.turn = 220.0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            usage(stdout);
            exit(0);
        } else if ((v = value_of(a, "--video"))) {
            opt.video = v;
        } else if ((v = value_of(a, "--symbols"))) {
            if (strcmp(v, "ascii") == 0) opt.symbols = AV_SYM_ASCII;
            else if (strcmp(v, "block") == 0) opt.symbols = AV_SYM_BLOCK;
            else if (strcmp(v, "fine") == 0) opt.symbols = AV_SYM_FINE;
            else bad_usage();
        } else if ((v = value_of(a, "--color"))) {
            if (strcmp(v, "mono") == 0) opt.color = AV_COLOR_MONO;
            else if (strcmp(v, "16") == 0) opt.color = AV_COLOR_16;
            else if (strcmp(v, "256") == 0) opt.color = AV_COLOR_256;
            else if (strcmp(v, "true") == 0) opt.color = AV_COLOR_TRUE;
            else bad_usage();
        } else if (strcmp(a, "--demo") == 0) {
            opt.demo = 1;
        } else if (strcmp(a, "--find-pak") == 0) {
            opt.find_pak = 1;
        } else if (strcmp(a, "--keytest") == 0) {
            opt.keytest = 1;
        } else if ((v = value_of(a, "--frames"))) {
            opt.frames = parse_long(v);
            opt.frames_given = 1;
        } else if ((v = value_of(a, "--cell"))) {
            if (parse_wxh(v, &opt.cell_w, &opt.cell_h) != 0 ||
                opt.cell_w < 1 || opt.cell_h < 1)
                bad_usage();
        } else if ((v = value_of(a, "--res"))) {
            int w, h;

            if (strcmp(v, "auto") != 0 && (parse_wxh(v, &w, &h) != 0 ||
                                           w < 1 || h < 1))
                bad_usage();
            opt.res = v;
        } else if ((v = value_of(a, "--fps"))) {
            opt.fps = (int)parse_long(v);
        } else if ((v = value_of(a, "--cells"))) {
            if (parse_wxh(v, &opt.cap_cols, &opt.cap_rows) != 0)
                bad_usage();
        } else if ((v = value_of(a, "--log"))) {
            opt.log = v;
        } else if (strcmp(a, "--sound") == 0) {
            opt.sound = 1;
        } else if (strcmp(a, "--no-sound") == 0) {
            opt.sound = 0;
        } else if (strcmp(a, "--no-pad") == 0) {
            opt.pad = 0;
        } else if ((v = value_of(a, "--mouse"))) {
            if (strcmp(v, "auto") == 0) opt.mouse.mode = AV_MOUSE_AUTO;
            else if (strcmp(v, "evdev") == 0) opt.mouse.mode = AV_MOUSE_EVDEV;
            else if (strcmp(v, "term") == 0) opt.mouse.mode = AV_MOUSE_TERM;
            else if (strcmp(v, "none") == 0) opt.mouse.mode = AV_MOUSE_NONE;
            else bad_usage();
        } else if ((v = value_of(a, "--mouse-dev"))) {
            opt.mouse.dev = v;
        } else if (strcmp(a, "--mouse-list") == 0) {
            opt.mouse_list = 1;
        } else if (strcmp(a, "--no-mouse") == 0) {
            opt.mouse.mode = AV_MOUSE_NONE;
        } else if ((v = value_of(a, "--mouse-sens"))) {
            opt.mouse.look.sens = parse_float(v);
        } else if ((v = value_of(a, "--mouse-edge"))) {
            opt.mouse.look.edge = parse_float(v);
            opt.mouse.edge_given = 1;
        } else if ((v = value_of(a, "--mouse-turn"))) {
            opt.mouse.look.turn = parse_float(v);
            opt.mouse.turn_given = 1;
        } else if (strcmp(a, "--mouse-invert") == 0) {
            opt.mouse.look.invert = 1;
        } else if (a[0] == '-' && a[1]) {
            bad_usage();
        } else if (!opt.data) {
            opt.data = a;
        } else {
            bad_usage();
        }
    }
    if (opt.demo && !opt.frames_given)
        opt.frames = 600;
}

/* ---- state dir and the saved Picture ---- */

static char state_dir[PATH_MAX];

static void find_state_dir(void)
{
    const char *s = getenv("XDG_STATE_HOME");

#ifdef _WIN32
    /* %LOCALAPPDATA%\aftershock, unless XDG_STATE_HOME says otherwise
     * (the tests set it on every platform). */
    if (!(s && *s) && (s = getenv("LOCALAPPDATA")) && *s) {
        snprintf(state_dir, sizeof(state_dir), "%s", s);
        return;
    }
#endif
    if (s && *s)
        snprintf(state_dir, sizeof(state_dir), "%s", s);
    else if ((s = getenv("HOME")) && *s)
        snprintf(state_dir, sizeof(state_dir), "%s/.local/state", s);
    else
        snprintf(state_dir, sizeof(state_dir), ".");
}

/* The last `av_picture "N"` (or av_picture N) in config.cfg, or -1. */
static int saved_picture(void)
{
    char path[PATH_MAX + 32], line[512];
    FILE *f;
    int found = -1;

    snprintf(path, sizeof(path), "%s/aftershock/config.cfg", state_dir);
    f = fopen(path, "r");
    if (!f)
        return -1;
    while (fgets(line, sizeof(line), f)) {
        char *p = line, *end;
        double v;

        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, "av_picture", 10) != 0 ||
            (p[10] != ' ' && p[10] != '\t'))
            continue;
        p += 10;
        while (*p == ' ' || *p == '\t' || *p == '"')
            p++;
        v = strtod(p, &end);
        if (end != p)
            found = (int)v;
    }
    fclose(f);
    return found;
}

/* ---- the canvas ---- */

static const av_backend *backend;
static int symbols;
static int shown_picture = -1;
static av_config cfg;
static int term_cols, term_rows;

/* Fits the canvas to the terminal: capped by --cells, the overlay's rows
 * kept free under it for backends that do not lay out text themselves,
 * centred. */
static void fit_canvas(void)
{
    int reserve = av_half_family(backend) ? 0 : av_overlay_reserved_rows();
    int cols = term_cols, rows = term_rows, cw, ch;

    if (opt.cap_cols > 0 && cols > opt.cap_cols)
        cols = opt.cap_cols;
    if (opt.cap_rows > 0 && rows > opt.cap_rows)
        rows = opt.cap_rows;
    rows -= reserve;
    if (cols < 1)
        cols = 1;
    if (rows < 1)
        rows = 1;
    cw = cfg.cell_w;
    ch = cfg.cell_h;
    av_config_init(&cfg);
    cfg.symbols = symbols;
    cfg.color = opt.color;
    cfg.cols = cols;
    cfg.rows = rows;
    cfg.cell_w = cw;
    cfg.cell_h = ch;
    /* The picture and the rows under it, as one block. */
    cfg.origin_col = 1 + (term_cols - cols) / 2;
    cfg.origin_row = 1 + (term_rows - (rows + reserve)) / 2;
    if (cfg.origin_col < 1)
        cfg.origin_col = 1;
    if (cfg.origin_row < 1)
        cfg.origin_row = 1;
}

/* Cell size in pixels: what the terminal said at setup, else --cell. */
static void find_cell_size(void)
{
    int pw, ph;

    cfg.cell_w = opt.cell_w;
    cfg.cell_h = opt.cell_h;
    if (av_term_text_pixels(&pw, &ph) && term_cols > 0 && term_rows > 0 &&
        pw / term_cols >= 1 && ph / term_rows >= 1) {
        cfg.cell_w = pw / term_cols;
        cfg.cell_h = ph / term_rows;
    }
}

static void choose_resolution(char *out, size_t cap)
{
    int w, h;

    if (strcmp(opt.res, "auto") != 0) {
        snprintf(out, cap, "%s", opt.res);
        return;
    }
    if (av_half_family(backend)) {
        av_half_resolution(cfg.cols, cfg.rows - av_overlay_reserved_rows(),
                           &w, &h);
    } else {
        w = cfg.cols * 2;
        h = cfg.rows * 4;
        if (w < 320) w = 320;
        if (h < 200) h = 200;
        if (w > 1920) w = 1920;
        if (h > 1200) h = 1200;
    }
    snprintf(out, cap, "%dx%d", w, h);
}

static void sleep_for(double s)
{
#ifdef _WIN32
    /* Sleep() is in milliseconds, and fine only with the 1 ms timer
     * period main() asks for. */
    if (s > 0 && !av_term_quit_requested())
        Sleep((DWORD)(s * 1000.0 + 0.5));
#else
    struct timespec ts;

    if (s <= 0)
        return;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
        if (av_term_quit_requested())
            break;
#endif
}

/* ---- the game ---- */

static double next_present;

/* Presenting is throttled, not the loop: the engine makes one frame's
 * worth of audio per run, so a slower loop starves the sound card. */
static int want_frame(void)
{
    double now, period;

    if (opt.fps <= 0)
        return 1;
    now = av_now();
    period = 1.0 / opt.fps;
    if (now < next_present)
        return 0;
    next_present = now - next_present > period ? now + period
                                               : next_present + period;
    return 1;
}

static void engine_frame(const uint8_t *rgb, int w, int h, int stride)
{
    const av_textui_t *ui = aftershock_textui();

    if (av_term_size(&term_cols, &term_rows)) {
        fit_canvas();
        av_pointer_resize(term_cols, term_rows);
        av_render_reconfigure(&cfg);
    }
    if (ui && ui->picture != shown_picture) {
        int s = symbols;
        const av_backend *b = av_picture_backend(ui->picture, &s);

        if (b) {
            av_host_log("picture: %d\n", ui->picture);
            shown_picture = ui->picture;
            backend = b;
            symbols = s;
            fit_canvas();
            av_render_switch(backend, &cfg);
        }
    }
    av_overlay_capture();
    av_render_submit(rgb, w, h, stride);
}

static void game_key(unsigned code, int down, unsigned mods)
{
    av_host_key(code, down, mods);
}

static int run_game(const char *pak)
{
    av_host_params hp;
    av_keys_sink ks;
    char res[32], err[256];
    double period, next;
    long frames = 0;

    av_term_size(&term_cols, &term_rows);
    fit_canvas();
    av_term_clear();
    backend->resize(&cfg);
    av_pointer_set_buttons(av_host_mouse_button);
    if (av_pointer_start(cfg.cell_w, cfg.cell_h, term_cols, term_rows, err,
                         sizeof(err)) != 0) {
        backend->shutdown();
        av_term_restore();
        fprintf(stderr, "aftershock: mouse: %s\n", err);
        return 2;
    }
    if (av_render_start(backend, &cfg) != 0) {
        av_pointer_stop();
        backend->shutdown();
        av_term_restore();
        fprintf(stderr, "aftershock: could not start the render thread\n");
        return 1;
    }
    choose_resolution(res, sizeof(res));

    av_host_log_open(opt.log);
    memset(&hp, 0, sizeof(hp));
#ifdef _WIN32
    /* The core finds the game folder by trimming the path at the
     * platform's own separator, so on Windows it must get backslashes:
     * with forward ones the folder name came out empty and only id1/
     * loaded. */
    {
        static char native[PATH_MAX + 64];
        size_t i;

        snprintf(native, sizeof(native), "%s", pak);
        for (i = 0; native[i]; i++)
            if (native[i] == '/')
                native[i] = '\\';
        pak = native;
    }
#endif
    hp.pak = pak;
    hp.state_dir = state_dir;
    hp.resolution = res;
    hp.sound = opt.sound;
    hp.want_frame = want_frame;
    hp.frame = engine_frame;
    if (av_host_start(&hp) != 0) {
        av_render_stop();
        av_pointer_stop();
        backend->shutdown();
        av_term_restore();
        fprintf(stderr, "aftershock: the engine could not load %s\n", pak);
        return 1;
    }

    memset(&ks, 0, sizeof(ks));
    ks.key = game_key;
    ks.text_input = aftershock_text_input;
    ks.mouse = av_pointer_report;
    ks.focus = av_pointer_focus;
    ks.release_toggle = av_pointer_toggle_release;
    av_keys_init(&ks, av_term_kitty());
    if (opt.pad)
        av_pad_open();
    if (av_pointer_note()[0])
        av_host_log("mouse: %s\n", av_pointer_note());

    period = 1.0 / av_host_fps();
    next = av_now();
    while (!av_term_quit_requested()) {
        int mx, my;
        double now;

        av_keys_poll();
        av_keys_expire();
        av_pointer_frame(av_now(), &mx, &my, NULL, NULL);
        if (mx || my)
            av_host_mouse_move(mx, my);
        av_pad_poll(av_pointer_focused());
        av_host_run();
        if (opt.frames > 0 && ++frames >= opt.frames)
            break;
        /* Behind: start the schedule again from now rather than racing
         * to catch up. */
        next += period;
        now = av_now();
        if (next > now)
            sleep_for(next - now);
        else
            next = now;
    }

    av_render_stop();
    av_pad_close();
    /* The pointer goes back before the slow part of shutdown. */
    av_pointer_stop();
    av_host_shutdown();
    return 0;
}

/* ---- --demo ---- */

static void demo_pattern(uint8_t *px, int w, int h, long frame)
{
    static const uint8_t bars[8][3] = {
        { 255, 255, 255 }, { 255, 255, 0 }, { 0, 255, 255 }, { 0, 255, 0 },
        { 255, 0, 255 }, { 255, 0, 0 }, { 0, 0, 255 }, { 0, 0, 0 },
    };
    int x, y, split = h * 2 / 3;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint8_t *p = px + ((size_t)y * w + x) * 3;

            if (y < split) {
                /* Saturated primaries in a known order: a channel-order
                 * mistake shows at once. */
                memcpy(p, bars[x * 8 / w], 3);
            } else {
                int t = (int)((x + frame * 2) % w) * 255 / (w - 1);

                p[0] = (uint8_t)t;
                p[1] = (uint8_t)(255 - t);
                p[2] = (uint8_t)((y - split) * 255 / (h - split - 1));
            }
        }
    }
}

static int run_demo(void)
{
    enum { W = 320, H = 200 };
    static uint8_t px[W * H * 3];
    av_keys_sink ks;
    long frame = 0;

    av_term_size(&term_cols, &term_rows);
    fit_canvas();
    av_term_clear();
    backend->resize(&cfg);
    memset(&ks, 0, sizeof(ks));
    av_keys_init(&ks, av_term_kitty());
    while (!av_term_quit_requested() &&
           (opt.frames <= 0 || frame < opt.frames)) {
        av_keys_poll();
        if (av_term_size(&term_cols, &term_rows)) {
            fit_canvas();
            av_term_clear();
            backend->resize(&cfg);
        }
        demo_pattern(px, W, H, frame);
        backend->frame(px, W, H, W * 3);
        frame++;
        sleep_for(0.033);
    }
    return 0;
}

/* ---- --keytest ---- */

static void kt_print(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

static void kt_print(const char *fmt, ...)
{
    char b[1024];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(b, sizeof(b) - 2, fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > (int)sizeof(b) - 3)
        n = (int)sizeof(b) - 3;
    /* OPOST is off: CR LF, or the lines walk to the right. */
    b[n++] = '\r';
    b[n++] = '\n';
    av_term_write_raw(b, (size_t)n);
}

static void kt_raw(const unsigned char *bytes, size_t n)
{
    char line[1024];
    size_t i, o = 0;

    o += (size_t)snprintf(line, sizeof(line), "read %zu:", n);
    for (i = 0; i < n && o + 8 < sizeof(line); i++) {
        unsigned char c = bytes[i];

        if (c >= 33 && c < 127)
            o += (size_t)snprintf(line + o, sizeof(line) - o, " %02x'%c'",
                                  c, c);
        else
            o += (size_t)snprintf(line + o, sizeof(line) - o, " %02x", c);
    }
    kt_print("%s", line);
}

static void kt_decoded(unsigned code, int down, unsigned mods)
{
    kt_print("  key %-10s %-4s mods=0x%x", av_keys_name(code),
             down ? "DOWN" : "UP", mods);
}

static void kt_mouse(const av_mouse_report *r)
{
    kt_print("  mouse button=%d x=%d y=%d %s%s", r->button, r->x, r->y,
             r->motion ? "motion" : "", r->release ? "release" : "");
    av_pointer_report(r);
}

static void kt_focus(int in)
{
    kt_print("  focus %s", in ? "in" : "out");
    av_pointer_focus(in);
}

static void kt_x10(void)
{
    kt_print("  X10 mouse report ignored (the terminal has no mode 1016)");
}

static void kt_toggle(void)
{
    av_pointer_toggle_release();
    kt_print("  pointer %s", av_pointer_focused() ? "toggled" : "toggled "
             "(unfocused)");
}

static void kt_button(int b, int down)
{
    kt_print("  pointer button %d %s", b, down ? "down" : "up");
}

static int run_keytest(void)
{
    av_keys_sink ks;
    char err[256];
    int pw = 0, ph = 0;

    av_term_size(&term_cols, &term_rows);
    av_pointer_set_buttons(kt_button);
    if (av_pointer_start(cfg.cell_w, cfg.cell_h, term_cols, term_rows, err,
                         sizeof(err)) != 0) {
        av_term_restore();
        fprintf(stderr, "aftershock: mouse: %s\n", err);
        return 2;
    }
    av_term_write_raw("\033[H", 3);
    kt_print("aftershock --keytest");
    kt_print("key releases: %s", av_term_kitty() ?
             "YES - kitty keyboard protocol active" :
             "NO - press-only fallback, holds are synthesised");
    kt_print("pointer: %s", av_pointer_note()[0] ? av_pointer_note() :
             "off (--mouse=none)");
    if (av_term_text_pixels(&pw, &ph))
        kt_print("text area: %dx%d pixels, %dx%d per cell", pw, ph,
                 cfg.cell_w, cfg.cell_h);
    else
        kt_print("text area: not reported, cells taken as %dx%d pixels",
                 cfg.cell_w, cfg.cell_h);
    kt_print("press keys (arrows, Enter, W, Ctrl) or move the pointer; "
             "Ctrl-Q, Ctrl-\\ or F10 quits");
    kt_print("%s", "");

    memset(&ks, 0, sizeof(ks));
    ks.raw = kt_raw;
    ks.decoded = kt_decoded;
    ks.mouse = kt_mouse;
    ks.focus = kt_focus;
    ks.x10 = kt_x10;
    ks.release_toggle = kt_toggle;
    av_keys_init(&ks, av_term_kitty());
    while (!av_term_quit_requested()) {
        int mx, my, x, y;
        double fx, fy;

        av_keys_poll();
        av_pointer_frame(av_now(), &mx, &my, &fx, &fy);
        if (mx || my) {
            if (av_look_last(&x, &y))
                kt_print("  look %+d %+d at %d,%d band %.2f %.2f", mx, my,
                         x, y, fx, fy);
            else
                kt_print("  look %+d %+d", mx, my);
        }
        sleep_for(0.020);
    }
    av_pointer_stop();
    return 0;
}

/* ---- start ---- */

int main(int argc, char **argv)
{
    char pak[PATH_MAX + 64];
    const char *explicit_data;
    int picture_arg = -1, status;

    parse_args(argc, argv);
    find_state_dir();
#ifdef _WIN32
    /* No start script on Windows (av starts aftershock.exe itself), so
     * the program does what asciiverse/aftershock does elsewhere: the
     * state folder exists and the session log goes there. */
    {
        static char log_path[PATH_MAX + 32];
        char dir[PATH_MAX + 16];

        snprintf(dir, sizeof(dir), "%s/aftershock", state_dir);
        _mkdir(state_dir);
        _mkdir(dir);
        if (!opt.log) {
            snprintf(log_path, sizeof(log_path), "%s/session.log", dir);
            opt.log = log_path;
        }
    }
    /* Frame pacing sleeps a few milliseconds at a time; the default
     * 15.6 ms timer period would round every one of them up. */
    timeBeginPeriod(1);
#endif

    if (opt.mouse_list) {
        av_evdev_list(stdout);
        return 0;
    }
    explicit_data = opt.data ? opt.data : getenv("AFTERSHOCK_DATA");
    if (explicit_data && !*explicit_data)
        explicit_data = NULL;
    if (opt.find_pak) {
        if (av_data_find(explicit_data, pak, sizeof(pak)) != 0)
            return 1;
        printf("%s\n", pak);
        return 0;
    }

    /* The backend, and the Picture the engine opens in. */
    symbols = opt.symbols;
    if (opt.video) {
        backend = av_backend_find(opt.video);
        if (!backend) {
            fprintf(stderr, "aftershock: no such video backend '%s' "
                    "(have: %s)\n", opt.video, av_backend_names());
            return 2;
        }
        picture_arg = av_picture_of(backend, symbols);
    } else {
        int saved = saved_picture(), s = symbols;
        const av_backend *b = av_picture_backend(saved, &s);

        backend = b ? b : &av_backend_half;
        if (b)
            symbols = s;
    }
    aftershock_pictures = av_pictures_mask();
    aftershock_picture_arg = picture_arg;
    shown_picture = av_picture_of(backend, symbols);

    if (!opt.demo && !opt.keytest &&
        av_data_find(explicit_data, pak, sizeof(pak)) != 0)
        return 2;

    if (opt.demo)
        opt.mouse.mode = AV_MOUSE_NONE;
    if (av_pointer_choose(&opt.mouse) != 0)
        return 2;

    if (av_term_init() != 0) {
        av_evdev_close();
        return 1;
    }
    av_term_size(&term_cols, &term_rows);
    av_config_init(&cfg);
    find_cell_size();
    fit_canvas();
    if (!opt.keytest && backend->init(&cfg) != 0) {
        av_term_restore();
        fprintf(stderr, "aftershock: the %s backend did not start\n",
                backend->name);
        return 1;
    }

    if (opt.keytest)
        status = run_keytest();
    else if (opt.demo)
        status = run_demo();
    else
        status = run_game(pak);

    /* The error paths shut down what they started themselves. */
    if (!opt.keytest && status == 0)
        backend->shutdown();
    {
        int kitty = av_term_kitty();

        av_term_restore();
        if (status == 0) {
            fprintf(stderr, "aftershock: video %s, key releases %s\n",
                    opt.keytest ? "none (--keytest)" : backend->name,
                    kitty ? "yes (kitty keyboard protocol)" :
                            "no (holds synthesised)");
            if (av_pointer_note()[0])
                fprintf(stderr, "aftershock: mouse: %s\n",
                        av_pointer_note());
        }
    }
    return status;
}
