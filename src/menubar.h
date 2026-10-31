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

/* menubar.h - the Viola-style menu bar: Game, Options, Help.
 *
 * Esc, F9 or F10 activate the bar; Alt+G / Alt+O / Alt+H open a menu
 * directly; a click on a title opens it. In an open menu: Left/Right switch
 * menus, Up/Down/Enter choose, the underlined letter chooses, Esc closes. */
#ifndef MENUBAR_H
#define MENUBAR_H

typedef enum {
    CMD_NONE,
    CMD_NEW_GAME, CMD_ROLL, CMD_BUILD, CMD_TRADE_BUY, CMD_TRADE_SELL, CMD_SELL_PACK,
    CMD_SINGLE_TAX, CMD_SCORES, CMD_NET_HOST, CMD_NET_JOIN, CMD_QUIT,
    CMD_SETTINGS, CMD_SPEED, CMD_TOGGLE_BIDS, CMD_TOGGLE_CONFIRM,
    CMD_HELP_PLAY, CMD_HELP_RULES, CMD_HELP_HISTORY, CMD_HELP_KEYS, CMD_LICENSE, CMD_ABOUT
} Command;

typedef struct {
    const char *label;      /* & marks the item's letter */
    const char *hint;       /* shortcut shown on the right, or NULL */
    Command cmd;
    int (*checked)(void);   /* a toggle: shows a mark when on (or NULL) */
    int (*enabled)(void);   /* grayed and unchoosable when it returns 0 (or NULL) */
} MenuItem;

enum { MENU_GAME, MENU_OPTIONS, MENU_HELP, NMENUS };

/* Height of the bar in pixels. */
int menubar_height(void);

/* Draw the bar; `active` is the marked title or -1. */
void menubar_draw(int active);

/* The menu whose title is at (x, y), or -1. */
int menubar_hit(int x, int y);

/* Run the bar starting at menu `m` (open if `open`). Returns the command
 * chosen or CMD_NONE. `redraw` draws the window under the menus. */
Command menubar_run(int m, int open, void (*redraw)(void));

/* The menu opened by Alt+letter `c`, or -1. */
int menubar_for_letter(int c);

#endif
