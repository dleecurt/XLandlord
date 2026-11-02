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

/* app.h - the program's state, shared by the window code. */
#ifndef APP_H
#define APP_H

#include "game.h"
#include "prefs.h"
#include "net.h"

typedef struct {
    Game game;
    Prefs prefs;
    int playing;            /* a game is on the board */
    int human_turn;         /* the human may act (his turn, before throwing) */
    int cpu_running;        /* computer players are moving */
    int highlight;          /* a space to outline on the board, or -1 */
    int dice_rolling;       /* the dice are tumbling (drawn without numbers) */
    int me;                 /* network joiner: my player number */
    int headless;           /* XLandlord --server: no window, so nothing is drawn */
} App;

extern App app;

/* For the menus. */
int app_can_act(void);
int app_can_propose(void);
int app_bids_on(void);
int app_confirm_on(void);

/* The humans who share this screen (1 for a network joiner). */
int app_local_humans(void);

/* What the host does with a joiner's ACTION (net_set_action_handler):
 * shared by the windowed host and the server. */
int app_host_action(Game *g, int p, const NetMsg *m);

/* The color of player p's checker and houses. */
int app_player_color(int p);

#endif
