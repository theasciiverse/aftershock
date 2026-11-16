/* Aftershock: terminal text drawn over the picture.
 *
 * The engine records the characters of its menus, console and centre
 * messages, and a summary of the player's state (third_party/tyrquake/
 * common/av_textui.c).  Here that becomes real terminal text: a status line
 * at the bottom, centre messages, notify lines, menus and the console, all
 * readable at 80x24 where the engine's own 8-pixel font is a smudge.
 *
 * Threads: av_overlay_capture() runs on the engine thread right after a
 * frame is finished and copies the engine's record under a lock; the
 * render thread takes the newest copy when it draws a frame.
 */
#include "av_overlay.h"
#include "oq_term.h"
#include "oq_retro.h"

#define AV_TEXTUI_FRONTEND
#include "av_textui.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static av_textui_t pending, shown;
static int have_pending, have_shown;

#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (b))

static const uint32_t ink_of[] = {
    RGB(232, 226, 210),     /* normal */
    RGB(236, 180, 86),      /* the font's gold */
    RGB(255, 150, 50),      /* titles */
    RGB(255, 222, 90),      /* cursor */
    RGB(140, 132, 120),     /* dim */
};

#define BAR_PAPER   RGB(22, 17, 14)
#define CON_PAPER   RGB(28, 20, 16)
#define C_LABEL     RGB(150, 140, 125)
#define C_VALUE     RGB(240, 234, 220)
#define C_LOW       RGB(255, 90, 70)
#define C_GREEN     RGB(110, 200, 90)
#define C_YELLOW    RGB(235, 200, 70)
#define C_RED       RGB(230, 80, 60)
#define C_SILVER    RGB(200, 205, 215)
#define C_GOLD      RGB(240, 190, 60)
#define C_OWNED     RGB(200, 190, 170)
#define C_ACTIVE    RGB(255, 200, 80)
#define C_TITLE     RGB(255, 150, 50)

void av_overlay_capture(void)
{
    const av_textui_t *ui = aftershock_textui();

    if (!ui)
        return;
    /* Session logging: about once a second, what the player sees. */
    {
        static unsigned n;
        static int last_menu = -1, last_inter = -1;

        if (++n % 60 == 0 || ui->menu != last_menu ||
            ui->intermission != last_inter) {
            oq_retro_log("state: level \"%s\" game=%d demo=%d menu=%d "
                         "console=%d intermission=%d health=%d armor=%d "
                         "weapon=%d ammo=%d kills=%d/%d secrets=%d/%d "
                         "time=%d\n", ui->level, ui->in_game, ui->demo,
                         ui->menu, ui->console, ui->intermission,
                         ui->health, ui->armor, ui->weapon, ui->ammo,
                         ui->kills, ui->total_kills, ui->secrets,
                         ui->total_secrets, ui->level_time);
            last_menu = ui->menu;
            last_inter = ui->intermission;
        }
    }
    pthread_mutex_lock(&lock);
    /* Only the used records: the array is large and mostly empty. */
    memcpy(&pending, ui, offsetof(av_textui_t, rec));
    memcpy(pending.rec, ui->rec, (size_t)ui->n * sizeof(ui->rec[0]));
    memcpy((char *)&pending + offsetof(av_textui_t, in_game),
           (const char *)ui + offsetof(av_textui_t, in_game),
           sizeof(av_textui_t) - offsetof(av_textui_t, in_game));
    have_pending = 1;
    pthread_mutex_unlock(&lock);
}

int av_overlay_reserved_rows(void)
{
    return 1;
}

/* ---- a cell grid we can draw into --------------------------------------- */

typedef struct {
    int cols, rows;
    uint32_t *top, *bot, *ch;
    size_t stride;          /* bytes between cells */
} grid;

#define AT(g, f, c, r) (*(uint32_t *)((char *)(g)->f + \
                         ((size_t)(r) * (g)->cols + (c)) * (g)->stride))

static uint32_t shade(uint32_t a, uint32_t b, int keep)
{
    unsigned r = (((a >> 16) & 255) + ((b >> 16) & 255)) / 2;
    unsigned gg = (((a >> 8) & 255) + ((b >> 8) & 255)) / 2;
    unsigned bb = ((a & 255) + (b & 255)) / 2;

    return RGB(r * keep / 100, gg * keep / 100, bb * keep / 100);
}

/* Put one character.  paper 0xffffffff means "the picture, darkened". */
static void put(grid *g, int c, int r, uint32_t ch, uint32_t ink,
                uint32_t paper)
{
    if (c < 0 || r < 0 || c >= g->cols || r >= g->rows)
        return;
    if (paper == 0xffffffff)
        paper = shade(AT(g, top, c, r), AT(g, bot, c, r), 35);
    AT(g, top, c, r) = ink;
    AT(g, bot, c, r) = paper;
    AT(g, ch, c, r) = ch ? ch : ' ';
}

static int put_str(grid *g, int c, int r, const char *s, uint32_t ink,
                   uint32_t paper)
{
    for (; *s; s++, c++)
        put(g, c, r, (unsigned char)*s, ink, paper);
    return c;
}

static void fill_row(grid *g, int r, int c0, int c1, uint32_t paper)
{
    int c;

    for (c = c0; c < c1; c++)
        put(g, c, r, ' ', paper, paper);
}

/* ---- the status line ---------------------------------------------------- */

static const char *const weapon_names[9] = {
    "", "Axe", "Shotgun", "Double Shotgun", "Spike Gun", "Spike Cannon",
    "Grenades", "Rockets", "Lightning Gun"
};
static const char *const ammo_names[5] = {
    "", "shells", "spikes", "rockets", "cells"
};

static int field(grid *g, int c, int r, const char *label, const char *value,
                 uint32_t vcolor)
{
    c = put_str(g, c, r, label, C_LABEL, BAR_PAPER);
    return put_str(g, c, r, value, vcolor, BAR_PAPER);
}

static void status_line(grid *g, const av_textui_t *u)
{
    int r = g->rows - 1, c = 1, i;
    char buf[64];

    fill_row(g, r, 0, g->cols, BAR_PAPER);
    if (!u->in_game) {
        put_str(g, 1, r, "AFTERSHOCK", C_TITLE, BAR_PAPER);
        put_str(g, 13, r, "Esc menu   ` console   F10 quit", C_LABEL,
                BAR_PAPER);
        return;
    }

    snprintf(buf, sizeof(buf), "%d", u->health);
    c = field(g, c, r, "Health ", buf, u->health <= 25 ? C_LOW : C_VALUE);
    c += 2;
    snprintf(buf, sizeof(buf), "%d", u->armor);
    c = field(g, c, r, "Armor ", buf,
              u->armor_kind == 3 ? C_RED : u->armor_kind == 2 ? C_YELLOW :
              u->armor_kind == 1 ? C_GREEN : C_VALUE);
    c += 2;
    if (u->weapon >= 1 && u->weapon <= 8) {
        c = put_str(g, c, r, weapon_names[u->weapon], C_ACTIVE, BAR_PAPER);
        if (u->ammo_kind) {
            snprintf(buf, sizeof(buf), " %d %s", u->ammo,
                     ammo_names[u->ammo_kind]);
            c = put_str(g, c, r, buf, u->ammo <= 5 ? C_LOW : C_VALUE,
                        BAR_PAPER);
        }
        c += 2;
    }
    for (i = 0; i < 8; i++) {
        if (u->weapons_owned & (1 << i)) {
            put(g, c, r, '1' + i, (u->weapon == i + 1) ? C_ACTIVE : C_OWNED,
                BAR_PAPER);
        } else {
            put(g, c, r, '.', RGB(80, 72, 64), BAR_PAPER);
        }
        c++;
    }
    c += 2;
    if (u->keys & 1)
        c = put_str(g, c, r, "Silver ", C_SILVER, BAR_PAPER);
    if (u->keys & 2)
        c = put_str(g, c, r, "Gold ", C_GOLD, BAR_PAPER);
    if (u->powerups & 1)
        c = put_str(g, c, r, "Sigil ", RGB(120, 160, 255), BAR_PAPER);
    if (u->powerups & 2)
        c = put_str(g, c, r, "Aegis ", RGB(255, 90, 90), BAR_PAPER);
    if (u->powerups & 4)
        c = put_str(g, c, r, "Eye ", RGB(200, 200, 120), BAR_PAPER);
    if (u->powerups & 8)
        c = put_str(g, c, r, "Suit ", C_GREEN, BAR_PAPER);

    /* Kills and secrets on the right, when there is room; in the attract
     * demos, how to begin. */
    if (u->demo)
        snprintf(buf, sizeof(buf), "AFTERSHOCK  Esc menu");
    else
        snprintf(buf, sizeof(buf), "Kills %d/%d  Secrets %d/%d", u->kills,
                 u->total_kills, u->secrets, u->total_secrets);
    if (c + (int)strlen(buf) + 2 < g->cols)
        put_str(g, g->cols - 1 - (int)strlen(buf), r, buf,
                u->demo ? C_TITLE : C_LABEL, BAR_PAPER);
}

/* A box of lines in the middle of the picture. */
static void center_box(grid *g, const char *const *lines, int n,
                       const uint32_t *inks)
{
    int w = 0, i, r0, c0;

    for (i = 0; i < n; i++)
        if ((int)strlen(lines[i]) > w)
            w = (int)strlen(lines[i]);
    w += 4;
    r0 = (g->rows - 1) / 2 - (n + 2) / 2;
    c0 = g->cols / 2 - w / 2;
    for (i = -1; i <= n; i++) {
        fill_row(g, r0 + 1 + i, c0, c0 + w, BAR_PAPER);
        if (i >= 0 && i < n)
            put_str(g, c0 + (w - (int)strlen(lines[i])) / 2, r0 + 1 + i,
                    lines[i], inks ? inks[i] : C_VALUE, BAR_PAPER);
    }
}

static void level_summary(grid *g, const av_textui_t *u, int done)
{
    char l1[80], l2[64], l3[64], l4[64];
    const char *lines[5];
    uint32_t inks[5] = { C_TITLE, C_VALUE, C_VALUE, C_VALUE, C_LABEL };
    int t = done ? u->completed_time : u->level_time;

    snprintf(l1, sizeof(l1), "%s", done ? "Level complete" : u->level);
    snprintf(l2, sizeof(l2), "Time      %d:%02d", t / 60, t % 60);
    snprintf(l3, sizeof(l3), "Kills     %d of %d", u->kills, u->total_kills);
    snprintf(l4, sizeof(l4), "Secrets   %d of %d", u->secrets,
             u->total_secrets);
    lines[0] = l1;
    lines[1] = l2;
    lines[2] = l3;
    lines[3] = l4;
    lines[4] = done ? "Fire to go on" : "";
    center_box(g, lines, done ? 5 : 4, inks);
}

/* ---- the engine's text -------------------------------------------------- */

/* Text inside a blanked box keeps the box's paper. */
static int is_boxed(grid *g, int c, int r)
{
    return c >= 0 && r >= 0 && c < g->cols && r < g->rows &&
           AT(g, ch, c, r) == ' ' && AT(g, bot, c, r) == BAR_PAPER &&
           AT(g, top, c, r) == BAR_PAPER;
}

static void engine_text(grid *g, const av_textui_t *u)
{
    int i, menu_c0, menu_r0, con_rows, center_y0 = -1;
    int pic_rows = g->rows - 1;
    int vw = u->vid_w > 0 ? u->vid_w : 320, vh = u->vid_h > 0 ? u->vid_h : 200;

    menu_c0 = g->cols / 2 - 20;
    menu_r0 = pic_rows > 25 ? (pic_rows - 25) / 2 : 0;

    /* The console gets a solid backing, so it reads as a console. */
    con_rows = u->console / 8;
    if (con_rows > pic_rows)
        con_rows = pic_rows;
    for (i = 0; i < con_rows; i++)
        fill_row(g, i, 0, g->cols, CON_PAPER);

    for (i = 0; i < u->n; i++) {
        const av_textrec_t *t = &u->rec[i];

        if (t->ctx == AV_CTX_CENTER && (center_y0 < 0 || t->y < center_y0))
            center_y0 = t->y;
    }

    for (i = 0; i < u->n; i++) {
        const av_textrec_t *t = &u->rec[i];
        uint32_t ink = ink_of[t->ink < 5 ? t->ink : 0];
        int c, r;

        switch (t->ctx) {
        case AV_CTX_MENU:
            c = menu_c0 + t->x / 8;
            r = menu_r0 + t->y / 8;
            if (r >= pic_rows)
                continue;
            if (t->ch == AV_CH_BLANK)
                put(g, c, r, ' ', BAR_PAPER, BAR_PAPER);
            else
                put(g, c, r, t->ch, ink,
                    is_boxed(g, c, r) ? BAR_PAPER : 0xffffffff);
            break;
        case AV_CTX_CONSOLE:
            c = t->x / 8 - 1;
            r = t->y / 8;
            if (r >= pic_rows)
                continue;
            put(g, c, r, t->ch, ink, r < con_rows ? CON_PAPER : 0xffffffff);
            break;
        case AV_CTX_CENTER:
            c = g->cols / 2 + (t->x - vw / 2) / 8;
            r = center_y0 * pic_rows / vh + (t->y - center_y0) / 8;
            if (r >= pic_rows)
                continue;
            put(g, c, r, t->ch, ink, 0xffffffff);
            break;
        }
    }
}

static void draw_all(grid *g, const av_textui_t *u)
{
    int pic_rows = g->rows - 1;

    status_line(g, u);
    if (u->in_game && !u->menu && u->console <= 0 && !u->intermission &&
        u->health > 0)
        put(g, g->cols / 2, pic_rows / 2, '+', RGB(255, 255, 255),
            0xffffffff);
    if (u->loading) {
        const char *l[1] = { "Loading" };
        center_box(g, l, 1, NULL);
        return;
    }
    if (u->intermission == 1) {
        level_summary(g, u, 1);
    } else if (u->showscores && u->in_game && !u->menu) {
        level_summary(g, u, 0);
    } else if (u->in_game && u->health <= 0 && !u->menu && !u->intermission) {
        const char *l[1] = { "Fire to try again" };
        center_box(g, l, 1, NULL);
    }
    if (u->paused && !u->menu) {
        const char *l[1] = { "Paused" };
        center_box(g, l, 1, NULL);
    }
    engine_text(g, u);
}

static int take_latest(void)
{
    pthread_mutex_lock(&lock);
    if (have_pending) {
        memcpy(&shown, &pending, offsetof(av_textui_t, rec));
        memcpy(shown.rec, pending.rec, (size_t)pending.n * sizeof(pending.rec[0]));
        memcpy((char *)&shown + offsetof(av_textui_t, in_game),
               (const char *)&pending + offsetof(av_textui_t, in_game),
               sizeof(av_textui_t) - offsetof(av_textui_t, in_game));
        have_pending = 0;
        have_shown = 1;
    }
    pthread_mutex_unlock(&lock);
    return have_shown;
}

void av_overlay_apply_cells(int cols, int rows, uint32_t *top, uint32_t *bot,
                            uint32_t *ch, size_t stride)
{
    grid g;

    if (!take_latest() || cols < 10 || rows < 3)
        return;
    g.cols = cols;
    g.rows = rows;
    g.top = top;
    g.bot = bot;
    g.ch = ch;
    g.stride = stride;
    draw_all(&g, &shown);
}

/* For backends that write their own picture (chafa, libcaca): draw the text
 * on a blank grid and send only the cells that hold text, after the frame. */
void av_overlay_emit(int cols, int rows, int origin_col, int origin_row)
{
    static uint32_t *cells;
    static size_t cap;
    size_t n = (size_t)cols * rows, i;
    grid g;
    char *out;
    size_t o = 0, ocap;
    uint32_t fg = 0xffffffff, bg = 0xffffffff;

    if (!take_latest() || cols < 10 || rows < 3)
        return;
    if (cap < n) {
        free(cells);
        cells = malloc(n * 3 * sizeof(uint32_t));
        cap = cells ? n : 0;
        if (!cells)
            return;
    }
    memset(cells, 0, n * 3 * sizeof(uint32_t));
    g.cols = cols;
    g.rows = rows;
    g.top = cells;
    g.bot = cells + 1;
    g.ch = cells + 2;
    g.stride = 3 * sizeof(uint32_t);
    draw_all(&g, &shown);

    ocap = n * 48 + 64;
    out = malloc(ocap);
    if (!out)
        return;
    o += (size_t)snprintf(out + o, ocap - o, "\033[?2026h");
    for (i = 0; i < n; i++) {
        uint32_t *k = cells + i * 3;
        uint32_t c = k[2];
        int r = (int)(i / cols), cc = (int)(i % cols);

        if (!c)
            continue;
        o += (size_t)snprintf(out + o, ocap - o, "\033[%d;%dH",
                              origin_row + r, origin_col + cc);
        if (k[0] != fg) {
            fg = k[0];
            o += (size_t)snprintf(out + o, ocap - o, "\033[38;2;%u;%u;%um",
                                  (fg >> 16) & 255, (fg >> 8) & 255, fg & 255);
        }
        if (k[1] != bg) {
            bg = k[1];
            o += (size_t)snprintf(out + o, ocap - o, "\033[48;2;%u;%u;%um",
                                  (bg >> 16) & 255, (bg >> 8) & 255, bg & 255);
        }
        if (c < 0x80) {
            out[o++] = (char)c;
        } else {
            out[o++] = '?';
        }
    }
    o += (size_t)snprintf(out + o, ocap - o, "\033[0m\033[?2026l");
    oq_term_write_raw(out, o);
    free(out);
}

int av_overlay_picture(void)
{
    int p = -1;

    pthread_mutex_lock(&lock);
    if (have_pending)
        p = pending.picture;
    else if (have_shown)
        p = shown.picture;
    pthread_mutex_unlock(&lock);
    return p;
}
