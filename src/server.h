/*
 * XLandlord - The Landlord's Game for X11
 * Copyright (C) 2026 Lee Curtis
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

/* server.h - XLandlord --server: host games with no window, game after
 * game, printing the record of events to the console and writing it to
 * ~/log/xlandlord-YYYY-MM-DD-HHMM.log (see server.c). */
#ifndef SERVER_H
#define SERVER_H

#include "prefs.h"

typedef struct {
    int port;               /* to listen on (127.0.0.1 only; joiners come through ssh) */
    int skill;              /* the computer player's DIFF_* */
    int end_wages;          /* 3, 5, 7 or 10 */
    int advanced;           /* the advanced rules */
    int reconnect_min;      /* minutes a dropped player has to come back: 1, 2 or 5 */
    int answer_min;         /* minutes to answer: 1, 2 or 5 */
    int start_wait;         /* seconds after the latest arrival before a game starts */
    char name[32];          /* the host's name, shown in the waiting rooms */
} ServerOptions;

/* The options from the preferences (port, skill, length, rules, timers),
 * a 60-second start wait, and the name "XLandlord server". */
void server_defaults(ServerOptions *o, const Prefs *p);

/* Read the server option at argv[i] (--port N, --skill LEVEL, --wages N,
 * --advanced, --standard, --reconnect MIN, --answer MIN, --start-wait
 * SECS, --name NAME). Returns how many arguments it took (1 or 2), 0 if
 * argv[i] is not a server option, -1 after printing an error. */
int server_parse(int argc, char **argv, int i, ServerOptions *o);

/* Serve until Ctrl+C or SIGTERM. Returns the exit status. */
int server_run(const ServerOptions *o);

#endif
