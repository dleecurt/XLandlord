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

int prefs_seat_difficulty(int kind)
{
    switch (kind) {
    case SEAT_EASY:   return DIFF_EASY;
    case SEAT_MEDIUM: return DIFF_MEDIUM;
    case SEAT_HARD:   return DIFF_HARD;
    default:          return -1;
    }
}

int prefs_seats_used(const Prefs *p)
{
    int n = 0;
    for (int i = 0; i < NSEATS; i++) n += p->seat[i] != SEAT_EMPTY;
    return n;
}

int prefs_humans(const Prefs *p)
{
    int n = 0;
    for (int i = 0; i < NSEATS; i++) n += p->seat[i] == SEAT_HUMAN;
    return n;
}

void prefs_defaults(Prefs *p)
{
    memset(p, 0, sizeof *p);
    const char *u = getenv("USER");
    copy_str(p->names[0], sizeof p->names[0], u && *u ? u : "Player 1");
    if (p->names[0][0] >= 'a' && p->names[0][0] <= 'z') p->names[0][0] = (char)(p->names[0][0] - 'a' + 'A');
    for (int i = 1; i < NSEATS; i++) snprintf(p->names[i], sizeof p->names[i], "Player %d", i + 1);
    /* One human against one computer player of medium strength. */
    p->seat[0] = SEAT_HUMAN;
    p->seat[1] = SEAT_MEDIUM;
    p->seat[2] = p->seat[3] = SEAT_EMPTY;
    p->end_wages = 5;
    p->advanced = 0;
    p->ask_bids = 1;
    p->confirm_cpu = 0;
    p->speed = 1;
    p->net_host[0] = '\0';
    p->net_port = 1904;
    copy_str(p->net_command, sizeof p->net_command, "viola-landlord");
    p->net_cpu = SEAT_MEDIUM;
}

static const char *const seat_word[SEAT_KINDS] = { "empty", "human", "easy", "medium", "hard", "network" };

static int seat_from_word(const char *w)
{
    for (int i = 0; i < SEAT_KINDS; i++) if (!strcmp(w, seat_word[i])) return i;
    return -1;
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
    int old_diff = SEAT_MEDIUM;
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
            old_diff = !strcmp(val, "easy") ? SEAT_EASY : !strcmp(val, "hard") ? SEAT_HARD : SEAT_MEDIUM;
            for (int i = 1; i < NSEATS; i++) if (p->seat[i] >= SEAT_EASY && p->seat[i] <= SEAT_HARD) p->seat[i] = old_diff;
        }
        else if (!strcmp(key, "opponents")) {
            n = CLAMP(n, 1, 3);
            for (int i = 1; i < NSEATS; i++) p->seat[i] = i <= n ? old_diff : SEAT_EMPTY;
        }
        else if (!strncmp(key, "seat", 4) && key[4] >= '1' && key[4] <= '0' + NSEATS && !key[5]) {
            int k = seat_from_word(val);
            if (k >= 0 && k != SEAT_NETWORK) p->seat[key[4] - '1'] = k;
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
        else if (!strcmp(key, "net_cpu")) { int k = seat_from_word(val); if (k >= SEAT_EASY && k <= SEAT_HARD) p->net_cpu = k; }
    }
    fclose(f);
    /* A table needs two players and someone at the screen. */
    if (prefs_seats_used(p) < 2 || prefs_humans(p) < 1) {
        Prefs d;
        prefs_defaults(&d);
        memcpy(p->seat, d.seat, sizeof p->seat);
    }
}

int prefs_save(const Prefs *p)
{
    char dir[600], path[700];
    if (!config_dir(dir, sizeof dir, 1)) return 0;
    snprintf(path, sizeof path, "%s/preferences.conf", dir);
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "# Viola the Landlord preferences (written by the program; edit freely)\n");
    fprintf(f, "# seatN = empty | human | easy | medium | hard; nameN = a human seat's name\n");
    for (int i = 0; i < NSEATS; i++) {
        fprintf(f, "seat%d = %s\n", i + 1, seat_word[CLAMP(p->seat[i], 0, SEAT_KINDS - 1)]);
        fprintf(f, "name%d = %s\n", i + 1, p->names[i]);
    }
    fprintf(f, "end_wages = %d\n", p->end_wages);
    fprintf(f, "advanced = %d\n", p->advanced);
    fprintf(f, "ask_bids = %d\n", p->ask_bids);
    fprintf(f, "confirm_cpu = %d\n", p->confirm_cpu);
    fprintf(f, "speed = %d\n", p->speed);
    fprintf(f, "# network play: the last host joined, the port to host on, the program ssh runs on\n"
               "# the host (give a full path if it is not on the host's PATH), the computer player\n");
    fprintf(f, "net_host = %s\n", p->net_host);
    fprintf(f, "net_port = %d\n", p->net_port);
    fprintf(f, "net_command = %s\n", p->net_command);
    fprintf(f, "net_cpu = %s\n", seat_word[CLAMP(p->net_cpu, SEAT_EASY, SEAT_HARD)]);
    return fclose(f) == 0;
}

int prefs_delay(const Prefs *p)
{
    static const int ms[] = { 150, 650, 1400 };
    return ms[CLAMP(p->speed, 0, 2)];
}
