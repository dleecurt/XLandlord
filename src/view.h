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

/* view.h - drawing the window: the menu bar, the board (after the layout of
 * Magie's 1904 patent drawing: a square circuit with the four corners, the
 * spaces between, and the boxes in the middle) and the side panel with the
 * players, the buttons and the record of events. */
#ifndef VIEW_H
#define VIEW_H

typedef enum { PB_NONE = -1, PB_ROLL, PB_BUILD, PB_TRADE, PB_SCORES, PB_COUNT } PanelButton;

/* Draw everything into the off-screen copy (call xw_flush to show it). */
void view_draw(void);

/* The board space at a window position, or -1. */
int view_space_at(int x, int y);

/* The panel button at a window position, or PB_NONE. */
PanelButton view_button_at(int x, int y);

/* A line of text shown above the buttons ("Your turn", "Mr. Hogg is
 * thinking..."). */
void view_set_status(const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

/* The button with the focus rectangle (what Enter presses), or PB_NONE. */
void view_set_focus(PanelButton b);

/* Scroll the record of events: +n older, -n newer, 0 = back to the newest. */
void view_scroll_log(int n);

#endif
