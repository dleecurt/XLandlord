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

/* netui.h - the window side of network play: the host's setup form (the
 * computer player's skill and the two timers) and lobby, the host's panel
 * while he waits for an absent or silent player (with Remove), the
 * joiner's Join dialog (asking another name if his is taken) and waiting
 * room, and the joiner's game: drawing what the host sends, answering its
 * questions, "Are you still there?" when his time to answer is up, and
 * reconnecting by himself when his connection drops. */
#ifndef NETUI_H
#define NETUI_H

#include "game.h"

/* Keep the window alive while the program waits on the network. Returns 1
 * if the player closed the window (and agreed to leave the game). */
int  netui_idle(void);
/* The host's panel while the game waits for a player (net_set_wait_hook). */
int  netui_wait(int p, int why, int secs);
/* The idle function under every dialog (xw_set_idle): the host polls its
 * connections; a joiner who has kept everyone waiting too long is asked
 * whether he is still there. Returns 1 if the screen needs redrawing. */
int  netui_tick(void);

/* Host: the setup form, then the lobby until the host starts or cancels.
 * Returns 1 with the table in *s (net_build_table) when the game should
 * begin; the caller creates it and calls net_start_game. */
int  netui_host_setup(GameSettings *s);

/* Join: ask for the host (an IP address or a name found in DNS or the hosts
 * file; `given`, from the command line, skips the question once), connect
 * through ssh, wait in the lobby. Returns 1 when the host has started the
 * game (app.game is set up for drawing). */
int  netui_join(const char *given);

/* Joiner: handle what the host has sent (state, records, questions, turns,
 * notes, the end), waiting at most ms for the first message. */
void netui_client_pump(int ms);

#endif
