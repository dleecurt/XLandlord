/*
 * Viola the Landlord - The Landlord's Game for X11
 * Copyright (C) 2026 Lee Curtis
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/* prefs.c - see prefs.h. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "prefs.h"
#include "game.h"
#include "util.h"

void prefs_defaults(Prefs *p)
{
    memset(p, 0, sizeof *p);
    const char *u = getenv("USER");
    copy_str(p->name, sizeof p->name, u && *u ? u : "Player");
    if (p->name[0] >= 'a' && p->name[0] <= 'z') p->name[0] = (char)(p->name[0] - 'a' + 'A');
    p->difficulty = DIFF_MEDIUM;
    p->opponents = 1;
    p->end_wages = 5;
    p->advanced = 0;
    p->ask_bids = 1;
    p->confirm_cpu = 0;
    p->speed = 1;
}

/* The directory, made if missing; returns 0 if there is no home. */
static int config_dir(char *out, size_t size, int make)
{
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    char base[512];
    if (xdg && *xdg) snprintf(base, sizeof base, "%s", xdg);
    else if (home && *home) snprintf(base, sizeof base, "%s/.config", home);
    else return 0;
    snprintf(out, size, "%s/viola-landlord", base);
    if (make) {
        mkdir(base, 0755);
        if (mkdir(out, 0755) != 0 && errno != EEXIST) return 0;
    }
    return 1;
}

void prefs_load(Prefs *p)
{
    char dir[600], path[700], line[256];
    prefs_defaults(p);
    if (!config_dir(dir, sizeof dir, 0)) return;
    snprintf(path, sizeof path, "%s/preferences.conf", dir);
    FILE *f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '=');
        if (line[0] == '#' || !eq) continue;
        *eq = '\0';
        char *key = line, *val = eq + 1;
        while (*key == ' ' || *key == '\t') key++;
        for (char *e = key + strlen(key); e > key && (e[-1] == ' ' || e[-1] == '\t'); ) *--e = '\0';
        while (*val == ' ' || *val == '\t') val++;
        val[strcspn(val, "\r\n")] = '\0';
        int n = atoi(val);
        if (!strcmp(key, "name") && *val) copy_str(p->name, sizeof p->name, val);
        else if (!strcmp(key, "difficulty")) {
            if (!strcmp(val, "easy")) p->difficulty = DIFF_EASY;
            else if (!strcmp(val, "hard")) p->difficulty = DIFF_HARD;
            else p->difficulty = DIFF_MEDIUM;
        }
        else if (!strcmp(key, "opponents")) p->opponents = CLAMP(n, 1, 3);
        else if (!strcmp(key, "end_wages")) p->end_wages = n == 3 || n == 7 || n == 10 ? n : 5;
        else if (!strcmp(key, "advanced")) p->advanced = n != 0;
        else if (!strcmp(key, "ask_bids")) p->ask_bids = n != 0;
        else if (!strcmp(key, "confirm_cpu")) p->confirm_cpu = n != 0;
        else if (!strcmp(key, "speed")) p->speed = CLAMP(n, 0, 2);
    }
    fclose(f);
}

int prefs_save(const Prefs *p)
{
    static const char *const dname[] = { "easy", "medium", "hard" };
    char dir[600], path[700];
    if (!config_dir(dir, sizeof dir, 1)) return 0;
    snprintf(path, sizeof path, "%s/preferences.conf", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "# Viola the Landlord preferences (written by the program; edit freely)\n");
    fprintf(f, "name = %s\n", p->name);
    fprintf(f, "difficulty = %s\n", dname[CLAMP(p->difficulty, 0, 2)]);
    fprintf(f, "opponents = %d\n", p->opponents);
    fprintf(f, "end_wages = %d\n", p->end_wages);
    fprintf(f, "advanced = %d\n", p->advanced);
    fprintf(f, "ask_bids = %d\n", p->ask_bids);
    fprintf(f, "confirm_cpu = %d\n", p->confirm_cpu);
    fprintf(f, "speed = %d\n", p->speed);
    return fclose(f) == 0;
}

int prefs_delay(const Prefs *p)
{
    static const int ms[] = { 150, 650, 1400 };
    return ms[CLAMP(p->speed, 0, 2)];
}
