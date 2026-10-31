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

/* prefs.h - the settings that last from one game to the next, kept in
 * $XDG_CONFIG_HOME/viola-landlord/preferences.conf (normally
 * ~/.config/viola-landlord/preferences.conf) as "key = value" lines. */
#ifndef PREFS_H
#define PREFS_H

typedef struct {
    char name[32];          /* the human player's name */
    int difficulty;         /* DIFF_EASY / DIFF_MEDIUM / DIFF_HARD */
    int opponents;          /* computer players, 1..3 */
    int end_wages;          /* the game ends at this many wages: 3, 5, 7 or 10 */
    int advanced;           /* the "advanced and scientific" rules */
    int ask_bids;           /* ask the human to bid when a computer player lands on an unowned card */
    int confirm_cpu;        /* wait for a key after each computer move */
    int speed;              /* 0 fast, 1 normal, 2 slow: the pause between computer moves */
} Prefs;

void prefs_defaults(Prefs *p);
void prefs_load(Prefs *p);
/* Returns 0 if the file could not be written. */
int  prefs_save(const Prefs *p);
/* The pause between computer moves, in milliseconds. */
int  prefs_delay(const Prefs *p);

#endif
