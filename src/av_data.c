/* Aftershock: finding the game data.
 *
 * Only our own layout counts: a folder holding aftershock/pak0.pak and
 * LibreQuake in id1/.  No Quake, QuakeSpasm or Steam location is ever
 * searched, so no id Software data is ever picked up.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "av_data.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_TRIED 8
#define TRIED_LEN (PATH_MAX + 32)

static int is_file(const char *p)
{
    struct stat st;

    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* dir/sub/pak0.pak or PAK0.PAK into out; 1 when found. */
static int find_pak(const char *dir, const char *sub, char *out, size_t cap)
{
    snprintf(out, cap, "%s/%s/pak0.pak", dir, sub);
    if (is_file(out))
        return 1;
    snprintf(out, cap, "%s/%s/PAK0.PAK", dir, sub);
    return is_file(out);
}

static int data_folder(const char *dir, char *out, size_t cap)
{
    char id1[PATH_MAX + 32];

    return find_pak(dir, "id1", id1, sizeof(id1)) &&
           find_pak(dir, "aftershock", out, cap);
}

static void report(const char *head, char tried[][TRIED_LEN], int n)
{
    int i;

    fprintf(stderr, "%s\nlooked in:\n", head);
    for (i = 0; i < n; i++)
        fprintf(stderr, "    %s\n", tried[i]);
    fprintf(stderr, "\nA data folder holds aftershock/pak0.pak and LibreQuake "
            "in id1/.\nIn a checkout: sh tools/fetch-librequake.sh && "
            "sh tools/build-data.sh\n");
}

int av_data_find(const char *explicit_path, char *out, size_t cap)
{
    char tried[MAX_TRIED][TRIED_LEN];
    char exe[PATH_MAX], head[PATH_MAX + 64];
    const char *s;
    ssize_t n;
    int nt = 0, i;

    if (explicit_path) {
        /* Honoured or refused, never replaced by a search hit. */
        if (is_file(explicit_path)) {
            snprintf(out, cap, "%s", explicit_path);
            return 0;
        }
        if (data_folder(explicit_path, out, cap))
            return 0;
        snprintf(tried[0], TRIED_LEN, "%s", explicit_path);
        snprintf(head, sizeof(head), "aftershock: no game data at '%s'",
                 explicit_path);
        report(head, tried, 1);
        return -1;
    }

    n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0) {
        char *slash;

        exe[n] = 0;
        slash = strrchr(exe, '/');
        if (slash) {
            *slash = 0;
            /* build/aftershock in a checkout: data/ is beside build/. */
            snprintf(tried[nt++], TRIED_LEN, "%s/../data", exe);
            snprintf(tried[nt++], TRIED_LEN, "%s/data", exe);
        }
    }
    snprintf(tried[nt++], TRIED_LEN, "./data");
    s = getenv("XDG_DATA_HOME");
    if (s && *s) {
        snprintf(tried[nt++], TRIED_LEN, "%s/aftershock", s);
    } else if ((s = getenv("HOME")) && *s) {
        snprintf(tried[nt++], TRIED_LEN, "%s/.local/share/aftershock", s);
    }
    snprintf(tried[nt++], TRIED_LEN, "/usr/share/aftershock");

    for (i = 0; i < nt; i++)
        if (data_folder(tried[i], out, cap))
            return 0;
    report("aftershock: could not find the game data", tried, nt);
    return -1;
}
