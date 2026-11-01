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

/* guidepics.h - the screenshots shown by Help > How to Play, compiled into
 * the program (src/guidepics.c, made by docs/guidepics; see there for the
 * run-length format). */
#ifndef GUIDEPICS_H
#define GUIDEPICS_H

#include <stddef.h>

typedef struct {
    const char *name;               /* the docs/guide/<name>.png it came from, '-' as '_' */
    int w, h;                       /* size in pixels */
    int ncolors;                    /* entries in palette */
    const unsigned long *palette;   /* 0xRRGGBB */
    const unsigned char *rle;       /* runs: color index, length-1 in 7-bit groups */
    size_t size;                    /* bytes in rle */
} GuidePic;

/* All of them, ended by an entry with a NULL name. */
extern const GuidePic guide_pics[];

#endif
