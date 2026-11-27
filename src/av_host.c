/* Aftershock: the libretro host.
 *
 * The core is linked in (STATIC_LINKING=1), so its retro_* functions are
 * called directly; this file answers its environment questions, expands
 * its RGB565 frames, feeds its audio to av_sound.c and answers its input
 * queries from state the main loop keeps up to date.
 *
 * Everything here runs on the engine thread except the log, which the
 * render thread may also use.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_host.h"
#include "av_sound.h"
#include "av_term.h"

#include <libretro.h>

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- session log ---- */

static FILE *logf_;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
static int engine_line_start = 1;

void av_host_log_open(const char *path)
{
    if (logf_ || !path)
        return;
    logf_ = fopen(path, "w");
}

void av_host_log_close(void)
{
    pthread_mutex_lock(&log_lock);
    if (logf_)
        fclose(logf_);
    logf_ = NULL;
    pthread_mutex_unlock(&log_lock);
}

void av_host_log(const char *fmt, ...)
{
    va_list ap;

    if (!logf_)
        return;
    pthread_mutex_lock(&log_lock);
    if (logf_) {
        va_start(ap, fmt);
        vfprintf(logf_, fmt, ap);
        va_end(ap);
        fflush(logf_);
    }
    pthread_mutex_unlock(&log_lock);
}

/* The core's logger.  The engine prints a line in several pieces, so the
 * level tag goes on the first piece of each line only. */
static void core_log(enum retro_log_level level, const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    size_t n;

    if (!logf_)
        return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    n = strlen(buf);
    if (!n)
        return;
    pthread_mutex_lock(&log_lock);
    if (logf_) {
        if (engine_line_start)
            fprintf(logf_, "[%d] ", (int)level);
        fputs(buf, logf_);
        fflush(logf_);
        engine_line_start = buf[n - 1] == '\n';
    }
    pthread_mutex_unlock(&log_lock);
}

/* ---- state ---- */

static av_host_params params;
static retro_keyboard_event_t key_cb;
static double fps = 60.0;
static unsigned sample_rate;
static int sound_open;
static int last_w, last_h;
static uint8_t *rgb;
static size_t rgb_cap;

static int mouse_dx, mouse_dy;
static int buttons_held[3];
static int wheel_latched[4];
static int arrow_held[4];       /* up, down, left, right */

/* ---- environment ---- */

static bool environment(unsigned cmd, void *data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *(const enum retro_pixel_format *)data ==
               RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
        key_cb = ((const struct retro_keyboard_callback *)data)->callback;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = core_log;
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;

        v->value = NULL;
        if (!v->key)
            return false;
        if (strcmp(v->key, "tyrquake_resolution") == 0)
            v->value = params.resolution;
        else if (strcmp(v->key, "tyrquake_framerate") == 0 ||
                 strcmp(v->key, "tyrquake_sound_samplerate") == 0)
            v->value = "auto";
        /* Disabled compute rendering keeps the software rasterizer, whose
         * 8-bit frame is what we draw. */
        else if (strcmp(v->key, "tyrquake_compute_rendering") == 0 ||
                 strcmp(v->key, "tyrquake_rumble") == 0 ||
                 strcmp(v->key, "tyrquake_invert_y_axis") == 0)
            v->value = "disabled";
        else if (strcmp(v->key, "tyrquake_analog_deadzone") == 0)
            v->value = "15";
        return v->value != NULL;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = false;
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = params.state_dir;
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)data = true;
        return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *(int *)data = 1 | 2;
        return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE: {
        const struct retro_message *m = data;

        if (m && m->msg)
            av_host_log("message: %s\n", m->msg);
        return true;
    }
    case RETRO_ENVIRONMENT_GET_MEMORY_STATUS: {
        /* The core takes a quarter: a 128 MB hunk, which LibreQuake's
         * larger maps need (the 64 MB default is not enough). */
        struct retro_memory_status *s = data;

        s->total = s->free = 512ull * 1024 * 1024;
        return true;
    }
    case RETRO_ENVIRONMENT_SHUTDOWN:
        /* The menu's Quit.  Not in the spec's list; the old build leaves
         * on it, and the loop watches this flag. */
        av_host_log("core: shutdown requested\n");
        av_term_request_quit();
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        return true;
    default:
        return false;
    }
}

/* ---- video ---- */

static void video(const void *data, unsigned w, unsigned h, size_t pitch)
{
    const uint8_t *src = data;
    size_t need = (size_t)w * h * 3;
    unsigned x, y;

    if (!data)
        return;     /* a dupe */
    if ((int)w != last_w || (int)h != last_h) {
        av_host_log("video: %ux%u\n", w, h);
        last_w = (int)w;
        last_h = (int)h;
    }
    if (params.want_frame && !params.want_frame())
        return;
    if (rgb_cap < need) {
        free(rgb);
        rgb = malloc(need);
        rgb_cap = rgb ? need : 0;
        if (!rgb)
            return;
    }
    for (y = 0; y < h; y++) {
        const uint16_t *row = (const uint16_t *)(src + y * pitch);
        uint8_t *o = rgb + (size_t)y * w * 3;

        for (x = 0; x < w; x++) {
            unsigned p = row[x], r = p >> 11, g = (p >> 5) & 63, b = p & 31;

            /* High bits copied into the low ones: full scale is 255. */
            *o++ = (uint8_t)((r << 3) | (r >> 2));
            *o++ = (uint8_t)((g << 2) | (g >> 4));
            *o++ = (uint8_t)((b << 3) | (b >> 2));
        }
    }
    if (params.frame)
        params.frame(rgb, (int)w, (int)h, (int)w * 3);
}

/* ---- audio ---- */

static void audio_sample(int16_t l, int16_t r)
{
    int16_t f[2] = { l, r };

    if (sound_open)
        av_sound_write(f, 1);
}

static size_t audio_batch(const int16_t *data, size_t frames)
{
    if (sound_open)
        av_sound_write(data, frames);
    return frames;
}

/* ---- input ---- */

static void input_poll(void)
{
}

static int16_t input_state(unsigned port, unsigned device, unsigned index,
                           unsigned id)
{
    int v;

    (void)index;
    if (port != 0)
        return 0;
    if (device == RETRO_DEVICE_MOUSE) {
        switch (id) {
        case RETRO_DEVICE_ID_MOUSE_X:
            v = mouse_dx;
            mouse_dx = 0;
            return (int16_t)v;
        case RETRO_DEVICE_ID_MOUSE_Y:
            v = mouse_dy;
            mouse_dy = 0;
            return (int16_t)v;
        case RETRO_DEVICE_ID_MOUSE_LEFT:   return (int16_t)buttons_held[0];
        case RETRO_DEVICE_ID_MOUSE_MIDDLE: return (int16_t)buttons_held[1];
        case RETRO_DEVICE_ID_MOUSE_RIGHT:  return (int16_t)buttons_held[2];
        case RETRO_DEVICE_ID_MOUSE_WHEELUP:             v = 0; break;
        case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:           v = 1; break;
        case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:       v = 2; break;
        case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:     v = 3; break;
        default:
            return 0;
        }
        /* One read sees the press; the next sees the release that ends
         * the bound command. */
        if (wheel_latched[v]) {
            wheel_latched[v] = 0;
            return 1;
        }
        return 0;
    }
    if (device == RETRO_DEVICE_KEYBOARD) {
        /* The core polls the arrows each frame and releases any reported
         * up: without this mirror a held arrow is let go at once (a held
         * left arrow turned 29.5 degrees with it, 1.4 without). */
        switch (id) {
        case RETROK_UP:    return (int16_t)arrow_held[0];
        case RETROK_DOWN:  return (int16_t)arrow_held[1];
        case RETROK_LEFT:  return (int16_t)arrow_held[2];
        case RETROK_RIGHT: return (int16_t)arrow_held[3];
        default:           return 0;
        }
    }
    return 0;
}

void av_host_key(unsigned code, int down, unsigned mods)
{
    av_host_log("key %u %s mods=%u\n", code, down ? "down" : "up", mods);
    switch (code) {
    case RETROK_UP:    arrow_held[0] = down; break;
    case RETROK_DOWN:  arrow_held[1] = down; break;
    case RETROK_LEFT:  arrow_held[2] = down; break;
    case RETROK_RIGHT: arrow_held[3] = down; break;
    default: break;
    }
    if (key_cb)
        key_cb(down != 0, code, 0, (uint16_t)mods);
}

void av_host_mouse_move(int dx, int dy)
{
    long x = (long)mouse_dx + dx, y = (long)mouse_dy + dy;

    /* The core reads an int16. */
    mouse_dx = (int)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
    mouse_dy = (int)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
}

void av_host_mouse_button(int button, int down)
{
    if (button >= 0 && button < 3)
        buttons_held[button] = down != 0;
    else if (button >= 3 && button < 7 && down)
        wheel_latched[button - 3] = 1;
}

/* ---- lifetime ---- */

int av_host_start(const av_host_params *p)
{
    struct retro_game_info game;
    struct retro_system_av_info av;

    params = *p;
    retro_set_environment(environment);
    retro_set_video_refresh(video);
    retro_set_audio_sample(audio_sample);
    retro_set_audio_sample_batch(audio_batch);
    retro_set_input_poll(input_poll);
    retro_set_input_state(input_state);
    retro_init();

    /* The core reads its options and registers the keyboard callback in
     * here, so the environment callback was set first. */
    memset(&game, 0, sizeof(game));
    game.path = params.pak;
    if (!retro_load_game(&game)) {
        av_host_log("core: could not load %s\n", params.pak);
        retro_deinit();
        return -1;
    }
    /* The core leaves port 0 on the joypad, and reads the mouse only on
     * the keyboard device. */
    retro_set_controller_port_device(0, RETRO_DEVICE_KEYBOARD);

    memset(&av, 0, sizeof(av));
    retro_get_system_av_info(&av);
    fps = av.timing.fps > 0 ? av.timing.fps : 60.0;
    sample_rate = av.timing.sample_rate > 0 ?
                  (unsigned)(av.timing.sample_rate + 0.5) : 44100;
    av_host_log("core: %.2f fps, %u Hz audio\n", fps, sample_rate);

    if (params.sound) {
        char err[256];

        if (av_sound_open(sample_rate, err, sizeof(err)) == 0) {
            sound_open = 1;
            av_host_log("audio: %u Hz on '%s'\n", sample_rate,
                        av_sound_device());
        } else {
            av_host_log("audio: disabled (%s)\n", err);
        }
    } else {
        av_host_log("audio: disabled (--no-sound)\n");
    }
    return 0;
}

double av_host_fps(void)
{
    return fps;
}

void av_host_run(void)
{
    retro_run();
}

void av_host_shutdown(void)
{
    retro_unload_game();
    retro_deinit();
    if (sound_open) {
        unsigned long d = av_sound_close();

        av_host_log("audio: %lu frames dropped\n", d);
        sound_open = 0;
    }
    free(rgb);
    rgb = NULL;
    rgb_cap = 0;
    av_host_log_close();
}
