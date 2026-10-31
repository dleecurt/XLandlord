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

/* keys.h - the key codes xw_next_key() returns.
 *
 * Plain characters are their ASCII codes (Enter is '\n', Esc 27, Tab '\t');
 * everything else is a code above 255. */
#ifndef KEYS_H
#define KEYS_H

#define ERR         (-1)

enum {
    K_UP = 256, K_DOWN, K_LEFT, K_RIGHT,
    K_HOME, K_END, K_PGUP, K_PGDN,
    K_BACKSPACE, K_DELETE, K_BTAB,
    K_MOUSE,        /* a mouse button: see xw_mouse */
    K_RESIZE,       /* the window changed size */
    K_CLOSE,        /* the window manager's close button */
    K_TIMEOUT,      /* xw_wait_key() timed out */
    K_F0 = 300,     /* K_F(1) .. K_F(24) */
    K_ALT0 = 400    /* K_ALT('a') .. Alt + a letter or digit */
};
#define K_F(n)      (K_F0 + (n))
#define K_ALT(c)    (K_ALT0 + (c))
#define IS_ALT(k)   ((k) >= K_ALT0 && (k) < K_ALT0 + 128)

#endif
