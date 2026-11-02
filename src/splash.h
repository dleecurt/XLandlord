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

/* splash.h - the title card shown when XLandlord starts (see splash.c). */
#ifndef SPLASH_H
#define SPLASH_H

/* The first question, asked on the title card. */
typedef enum { SPLASH_HOST, SPLASH_JOIN, SPLASH_LOCAL, SPLASH_QUIT, SPLASH_COUNT } SplashChoice;

/* The title card with the first question on it: Host a Network Game,
 * Join a Network Game, Play Here or Quit (H / J / P / Q, the arrows and
 * Enter, or a click; Esc moves to Quit; closing the window is Quit). */
SplashChoice splash_ask(void);

/* The title card alone with `note` where the choices would be, for `ms`
 * milliseconds or until a key or click (when --host, --join or --local on
 * the command line has already made the choice). */
void splash_show(const char *note, int ms);

#endif
