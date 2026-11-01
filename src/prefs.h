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

/* prefs.h - the settings that last from one game to the next, kept in
 * $XDG_CONFIG_HOME/xlandlord/preferences.conf (normally
 * ~/.config/xlandlord/preferences.conf) as "key = value" lines. A file
 * left by 0.0.6 or earlier, when the game was "Viola the Landlord", in
 * viola-landlord/preferences.conf, is read once and saved here. */
#ifndef PREFS_H
#define PREFS_H

/* Who sits in each of the four seats. */
typedef enum {
    SEAT_EMPTY,             /* nobody: the seat is skipped */
    SEAT_HUMAN,             /* a human at this screen (hot-seat) */
    SEAT_EASY,              /* computer players */
    SEAT_MEDIUM,
    SEAT_HARD,
    SEAT_NETWORK,           /* a human on another machine: reserved for network play (net.h) */
    SEAT_KINDS
} SeatKind;

#define NSEATS 4

typedef struct {
    int seat[NSEATS];       /* SeatKind of each seat, in turn order */
    char names[NSEATS][32]; /* the names of the human seats */
    int end_wages;          /* the game ends at this many wages: 3, 5, 7 or 10 */
    int advanced;           /* the "advanced and scientific" rules */
    int ask_bids;           /* ask the human to bid when a computer player lands on an unowned card */
    int confirm_cpu;        /* wait for a key after each computer move */
    int speed;              /* 0 fast, 1 normal, 2 slow: the pause between computer moves */
    /* Network play (net.h). */
    char net_host[128];     /* the last host joined: [user@]host[:port] */
    int net_port;           /* the port a hosted game listens on (localhost only) */
    char net_command[128];  /* the program ssh runs on the host for the relay */
    int net_cpu;            /* SEAT_EASY..SEAT_HARD: the computer player in a hosted game */
} Prefs;

/* The seat's difficulty (DIFF_*) if it is a computer player, else -1. */
int  prefs_seat_difficulty(int kind);
/* The number of seats that are not empty, and of those that are human. */
int  prefs_seats_used(const Prefs *p);
int  prefs_humans(const Prefs *p);

void prefs_defaults(Prefs *p);
void prefs_load(Prefs *p);
/* Returns 0 if the file could not be written. */
int  prefs_save(const Prefs *p);
/* The pause between computer moves, in milliseconds. */
int  prefs_delay(const Prefs *p);

#endif
