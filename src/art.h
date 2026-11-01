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

/* art.h - the game's pictures.
 *
 * Every picture here is original pixel art drawn for XLandlord as
 * rows of characters ('#' ink, '+' a second color, anything else clear), and
 * is distributed under the same license as the program (GPL-3.0-or-later);
 * the author also dedicates them to the public domain (CC0 1.0) for use
 * anywhere. The board itself is drawn from the layout of Lizzie J. Magie's
 * 1904 patent drawing (US 748,626), which is in the public domain. */
#ifndef ART_H
#define ART_H

typedef enum {
    ART_GLOBE, ART_JAIL, ART_TREE, ART_POORHOUSE,
    ART_TRAIN, ART_BULB, ART_TROLLEY, ART_CHANCE,
    ART_DIAMOND, ART_BREAD, ART_FLAME, ART_ROOF,
    ART_SHIRT, ART_SIGN, ART_DICE, ART_SCROLL,
    ART_COLLEGE, ART_HOUSE,
    ART_COUNT
} ArtId;

/* The rows of a picture (NULL-terminated) and its size in characters. */
const char *const *art_rows(ArtId id);
int art_width(ArtId id);
int art_height(ArtId id);

/* The window icon: 32 x 32 pixels, as 0xAARRGGBB. */
#define ICON_SIZE 32
unsigned long art_icon_argb(int x, int y);

#endif
