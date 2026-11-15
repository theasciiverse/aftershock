#include "oq_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATHCAP 1024

static int is_file(const char *p)
{
    struct stat st;

    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

static int is_dir(const char *p)
{
    struct stat st;

    return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static void note(char *tried, size_t cap, const char *p)
{
    size_t len = strlen(tried);

    if (len + strlen(p) + 8 < cap)
        snprintf(tried + len, cap - len, "    %s\n", p);
}

/* The shareware archive ships ID1/PAK0.PAK; a hand-made directory is usually
 * lowercase.  Try both rather than making the player rename anything. */
static int pak_in(const char *dir, char *out, size_t cap)
{
    static const char *names[] = { "pak0.pak", "PAK0.PAK" };
    size_t i;

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        snprintf(out, cap, "%s/%s", dir, names[i]);
        if (is_file(out))
            return 1;
    }
    out[0] = '\0';
    return 0;
}

static void exe_dir(char *out, size_t cap)
{
    ssize_t n = readlink("/proc/self/exe", out, cap - 1);
    char *slash;

    if (n <= 0) {
        out[0] = '\0';
        return;
    }
    out[n] = '\0';
    slash = strrchr(out, '/');
    if (slash)
        *slash = '\0';
}

/* Aftershock: the game is data/aftershock/pak0.pak (ours) over data/id1
 * (LibreQuake).  Only Aftershock's own places are searched: never a Quake
 * or Steam folder, so no id Software data is ever picked up.  A data folder
 * counts only when it has both halves. */
static int try_data(const char *dir, char *out, size_t cap, char *tried,
                    size_t triedcap)
{
    char sub[PATHCAP], lq[PATHCAP], lqpak[PATHCAP + 16];

    if (tried)
        note(tried, triedcap, dir);
    snprintf(sub, sizeof(sub), "%s/aftershock", dir);
    snprintf(lq, sizeof(lq), "%s/id1", dir);
    if (is_dir(sub) && is_dir(lq) && pak_in(sub, out, cap) &&
        pak_in(lq, lqpak, sizeof(lqpak)))
        return 1;
    out[0] = '\0';
    return 0;
}

int oq_find_pak(const char *want, char *out, size_t outcap,
                char *tried, size_t triedcap)
{
    const char *home = getenv("HOME");
    const char *xdg = getenv("XDG_DATA_HOME");
    char buf[PATHCAP + 16], exe[PATHCAP];

    if (tried && triedcap)
        tried[0] = '\0';

    if (!want || !*want)
        want = getenv("AFTERSHOCK_DATA");
    if (want && *want) {
        /* An explicit choice is honoured or refused, never silently ignored
         * in favour of a search. */
        if (is_file(want)) {
            snprintf(out, outcap, "%s", want);
            return 0;
        }
        return try_data(want, out, outcap, tried, triedcap) ? 0 : -1;
    }

    exe_dir(exe, sizeof(exe));
    if (exe[0]) {
        /* build/omaquake in a checkout, or an installed tree */
        snprintf(buf, sizeof(buf), "%s/../data", exe);
        if (try_data(buf, out, outcap, tried, triedcap))
            return 0;
        snprintf(buf, sizeof(buf), "%s/data", exe);
        if (try_data(buf, out, outcap, tried, triedcap))
            return 0;
    }
    if (try_data("data", out, outcap, tried, triedcap))
        return 0;
    if (xdg && *xdg) {
        snprintf(buf, sizeof(buf), "%s/aftershock", xdg);
        if (try_data(buf, out, outcap, tried, triedcap))
            return 0;
    } else if (home && *home) {
        snprintf(buf, sizeof(buf), "%s/.local/share/aftershock", home);
        if (try_data(buf, out, outcap, tried, triedcap))
            return 0;
    }
    if (try_data("/usr/share/aftershock", out, outcap, tried, triedcap))
        return 0;
    out[0] = '\0';
    return -1;
}
