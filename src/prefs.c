/*
 * XLandlord - The Landlord's Game for X11
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

/* See prefs.h. */
int prefs_computers(const Prefs *p)
{
    int n = 0;
    for (int i = 0; i < NSEATS; i++) n += p->seat[i] == SEAT_COMPUTER;
    return n;
}

/* See prefs.h. */
int prefs_seats_used(const Prefs *p)
{
    int n = 0;
    for (int i = 0; i < NSEATS; i++) n += p->seat[i] != SEAT_EMPTY;
    return n;
}

/* See prefs.h. */
int prefs_humans(const Prefs *p)
{
    int n = 0;
    for (int i = 0; i < NSEATS; i++) n += p->seat[i] == SEAT_HUMAN;
    return n;
}

/* See prefs.h: one human against one medium computer player, five wages,
 * standard rules. The human's name starts empty: it is never taken from
 * the system's user name - the player gives it when the game starts. */
void prefs_defaults(Prefs *p)
{
    memset(p, 0, sizeof *p);
    for (int i = 1; i < NSEATS; i++) snprintf(p->names[i], sizeof p->names[i], "Player %d", i + 1);
    /* One human against one computer player of medium strength. */
    p->seat[0] = SEAT_HUMAN;
    p->seat[1] = SEAT_COMPUTER;
    p->seat[2] = p->seat[3] = SEAT_EMPTY;
    p->end_wages = 5;
    p->advanced = 0;
    p->ask_bids = 1;
    p->confirm_cpu = 0;
    p->speed = 1;
    p->net_host[0] = '\0';
    p->net_port = 1904;
    copy_str(p->net_command, sizeof p->net_command, "XLandlord");
    p->net_reconnect = 2;
    p->net_answer = 2;
    p->cpu_skill = DIFF_MEDIUM;
}

static const char *const seat_word[SEAT_KINDS] = { "empty", "human", "computer", "network" };
static const char *const skill_word[3] = { "easy", "medium", "hard" };   /* DIFF_* */

/* DIFF_* from "easy" / "medium" / "hard", or -1. */
static int skill_from_word(const char *w)
{
    for (int i = 0; i < 3; i++) if (!strcmp(w, skill_word[i])) return i;
    return -1;
}

/* A seat kind from its word in the file ("human", "hard"...), or -1. */
static int seat_from_word(const char *w)
{
    for (int i = 0; i < SEAT_KINDS; i++) if (!strcmp(w, seat_word[i])) return i;
    return -1;
}

/* The directory, made if missing; returns 0 if there is no home.
 * `name` is the folder under the config base: "xlandlord", or the
 * folder of the game's former name when reading old preferences. */
static int config_dir_named(char *out, size_t size, int make, const char *name)
{
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    char base[512];
    if (xdg && *xdg) snprintf(base, sizeof base, "%s", xdg);
    else if (home && *home) snprintf(base, sizeof base, "%s/.config", home);
    else return 0;
    snprintf(out, size, "%s/%s", base, name);
    if (make) {
        mkdir(base, 0755);
        if (mkdir(out, 0755) != 0 && errno != EEXIST) return 0;
    }
    return 1;
}

/* This game's own preferences directory (see config_dir_named). */
static int config_dir(char *out, size_t size, int make)
{
    return config_dir_named(out, size, make, "xlandlord");
}

/* See prefs.h. Unknown keys are ignored. Older files are converted: 0.0.1
 * (name, difficulty, opponents) and 0.0.2 to 0.0.9 (a level for each
 * computer seat, net_cpu for hosted games) become one computer seat at
 * most, with its level as cpu_skill. */
void prefs_load(Prefs *p)
{
    char dir[600], path[700], line[256];
    prefs_defaults(p);
    if (!config_dir(dir, sizeof dir, 0)) return;
    snprintf(path, sizeof path, "%s/preferences.conf", dir);
    FILE *f = fopen(path, "r");
    int migrated = 0;
    if (!f) {
        /* Up to 0.0.6 the game was "Viola the Landlord" and kept its file
         * in viola-landlord/; read that once and save it under the new
         * name (below). */
        if (!config_dir_named(dir, sizeof dir, 0, "viola-landlord")) return;
        snprintf(path, sizeof path, "%s/preferences.conf", dir);
        if (!(f = fopen(path, "r"))) return;
        migrated = 1;
    }
    int skill_given = 0;        /* a cpu_skill line was read: it wins over older keys */
    int seat_skill = 0;         /* the first old easy/medium/hard seat gave the skill */
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
        /* 0.0.1 files: one human ("name") against `opponents` computer
         * players of one `difficulty`. */
        if (!strcmp(key, "name") && *val) copy_str(p->names[0], sizeof p->names[0], val);
        else if (!strcmp(key, "difficulty")) {
            int d = skill_from_word(val);
            if (d >= 0 && !skill_given) p->cpu_skill = d;
        }
        else if (!strcmp(key, "opponents")) {
            for (int i = 1; i < NSEATS; i++) p->seat[i] = i == 1 ? SEAT_COMPUTER : SEAT_EMPTY;
        }
        else if (!strncmp(key, "seat", 4) && key[4] >= '1' && key[4] <= '0' + NSEATS && !key[5]) {
            int k = seat_from_word(val), d = skill_from_word(val);
            if (d >= 0) {               /* 0.0.2 - 0.0.9: easy / medium / hard seats */
                k = SEAT_COMPUTER;
                if (!skill_given && !seat_skill) { p->cpu_skill = d; seat_skill = 1; }
            }
            if (k >= 0 && k != SEAT_NETWORK) p->seat[key[4] - '1'] = k;
        }
        else if (!strcmp(key, "cpu_skill")) {
            int d = skill_from_word(val);
            if (d >= 0) { p->cpu_skill = d; skill_given = 1; }
        }
        else if (!strncmp(key, "name", 4) && key[4] >= '1' && key[4] <= '0' + NSEATS && !key[5] && *val)
            copy_str(p->names[key[4] - '1'], sizeof p->names[0], val);
        else if (!strcmp(key, "end_wages")) p->end_wages = n == 3 || n == 7 || n == 10 ? n : 5;
        else if (!strcmp(key, "advanced")) p->advanced = n != 0;
        else if (!strcmp(key, "ask_bids")) p->ask_bids = n != 0;
        else if (!strcmp(key, "confirm_cpu")) p->confirm_cpu = n != 0;
        else if (!strcmp(key, "speed")) p->speed = CLAMP(n, 0, 2);
        else if (!strcmp(key, "net_host")) copy_str(p->net_host, sizeof p->net_host, val);
        else if (!strcmp(key, "net_port") && n > 0 && n < 65536) p->net_port = n;
        else if (!strcmp(key, "net_command") && *val) copy_str(p->net_command, sizeof p->net_command, val);
        else if (!strcmp(key, "net_reconnect")) p->net_reconnect = n == 1 || n == 5 ? n : 2;
        else if (!strcmp(key, "net_answer")) p->net_answer = n == 1 || n == 5 ? n : 2;
        else if (!strcmp(key, "net_host_only")) p->net_host_only = n != 0;
        else if (!strcmp(key, "net_cpu")) {     /* 0.0.3 - 0.0.9: used if no seat gave a level */
            int d = skill_from_word(val);
            if (d >= 0 && !skill_given && !seat_skill) p->cpu_skill = d;
        }
    }
    fclose(f);
    /* One computer player at most: later computer seats become empty. */
    for (int i = 0, seen = 0; i < NSEATS; i++)
        if (p->seat[i] == SEAT_COMPUTER && ++seen > MAX_CPU_PLAYERS) p->seat[i] = SEAT_EMPTY;
    /* A table needs two players and someone at the screen. */
    if (prefs_seats_used(p) < 2 || prefs_humans(p) < 1) {
        Prefs d;
        prefs_defaults(&d);
        memcpy(p->seat, d.seat, sizeof p->seat);
    }
    if (migrated) {
        /* The old default relay command was the old program name. */
        if (!strcmp(p->net_command, "viola-landlord")) copy_str(p->net_command, sizeof p->net_command, "XLandlord");
        prefs_save(p);
    }
}

/* See prefs.h: the whole file, rewritten. */
int prefs_save(const Prefs *p)
{
    char dir[600], path[700];
    if (!config_dir(dir, sizeof dir, 1)) return 0;
    snprintf(path, sizeof path, "%s/preferences.conf", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "# XLandlord preferences (written by the program; edit freely)\n");
    fprintf(f, "# seatN = empty | human | computer (one computer seat at most); nameN = a human seat's name\n");
    for (int i = 0; i < NSEATS; i++) {
        fprintf(f, "seat%d = %s\n", i + 1, seat_word[CLAMP(p->seat[i], 0, SEAT_KINDS - 1)]);
        fprintf(f, "name%d = %s\n", i + 1, p->names[i]);
    }
    fprintf(f, "end_wages = %d\n", p->end_wages);
    fprintf(f, "advanced = %d\n", p->advanced);
    fprintf(f, "# the computer player's strength, here and in hosted games: easy | medium | hard\n");
    fprintf(f, "cpu_skill = %s\n", skill_word[CLAMP(p->cpu_skill, DIFF_EASY, DIFF_HARD)]);
    fprintf(f, "ask_bids = %d\n", p->ask_bids);
    fprintf(f, "confirm_cpu = %d\n", p->confirm_cpu);
    fprintf(f, "speed = %d\n", p->speed);
    fprintf(f, "# network play: the last host joined, the port to host on, the program ssh runs on\n"
               "# the host (give a full path if it is not on the host's PATH)\n");
    fprintf(f, "net_host = %s\n", p->net_host);
    fprintf(f, "net_port = %d\n", p->net_port);
    fprintf(f, "net_command = %s\n", p->net_command);
    fprintf(f, "# hosted games: minutes to wait for a dropped player, and to answer (1, 2 or 5)\n");
    fprintf(f, "net_reconnect = %d\n", p->net_reconnect);
    fprintf(f, "net_answer = %d\n", p->net_answer);
    fprintf(f, "# hosted games: 1 = host only (watch; four seats for joiners), 0 = host and play\n");
    fprintf(f, "net_host_only = %d\n", p->net_host_only);
    return fclose(f) == 0;
}

/* See prefs.h. */
int prefs_delay(const Prefs *p)
{
    static const int ms[] = { 150, 650, 1400 };
    return ms[CLAMP(p->speed, 0, 2)];
}
