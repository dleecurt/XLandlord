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

/* xwin.h - the ONLY module that talks to the X server.
 *
 * Everything on screen is drawn by the program itself, the way the
 * standalone Viola browser draws its widgets (and as Ted Viola does): flat
 * gray94 surfaces, 2px light-and-dark bevels, 1px black boxes. There is no
 * toolkit. This module offers the few primitives the rest needs:
 *
 *   - one top-level window, drawn into an off-screen copy (a Pixmap) and
 *     shown with xw_flush(), so redrawing everything never flickers;
 *   - the Viola palette with its bevels and boxes, and the few muted colors
 *     the 1904 patent asks for on the board (green lots, blue necessities,
 *     yellow franchises, purple luxuries) and the players' checkers;
 *   - X core fonts, like Viola's, with fallbacks so the game still starts
 *     on a server that lacks them;
 *   - keyboard and mouse input (keys.h), with a timed wait so the computer
 *     players' moves can be watched;
 *   - the program's icon, for the window manager.
 *
 * Coordinates are pixels from the window's top-left corner. Text is drawn
 * from its BASELINE (y), as X does it. */
#ifndef XWIN_H
#define XWIN_H

#include <stddef.h>

enum {
    /* The Viola design system's palette. */
    XC_SURFACE,         /* gray94: every pane, menu and button */
    XC_INK,             /* black: text, boxes, the menu triangle */
    XC_LIGHT,           /* bevel-light (top/left of a raised border) */
    XC_DARK,            /* bevel-dark (bottom/right); also edit fields */
    XC_TROUGH,          /* trough: scroll troughs, popup outlines */
    XC_MARK,            /* LemonChiffon1: Viola's one tint (the current player) */
    /* The board, after the patent's "preferably" colors, softened to sit
     * quietly on gray94. */
    XC_LOT,             /* green */
    XC_NECESSITY,       /* blue */
    XC_FRANCHISE,       /* yellow */
    XC_RAILROAD,        /* pink */
    XC_LUXURY,          /* purple */
    XC_CHANCE,          /* a pale red, for the red pack */
    XC_TRESPASS,        /* a pale gray-brown: land held out of use */
    XC_CORNER,          /* the corners: plain paper */
    /* The four players' checkers and houses. */
    XC_PLAYER0, XC_PLAYER1, XC_PLAYER2, XC_PLAYER3,
    XC_WHITE,
    XC_COUNT
};

enum { XF_UI, XF_SMALL, XF_BOLD, XF_FIXED, XF_LOGO, XF_COUNT };

/* Connect, load fonts and colors. Returns 0 (after saying why) on failure. */
int  xw_open(void);
void xw_show(int argc, char **argv, int w, int h);
void xw_close(void);
int  xw_width(void);
int  xw_height(void);
void xw_set_title(const char *title);
void xw_flush(void);
void xw_bell(void);

/* ---- drawing (into the off-screen copy) ---- */
void xw_fill(int x, int y, int w, int h, int color);
void xw_hline(int x, int y, int w, int color);
void xw_vline(int x, int y, int h, int color);
void xw_line(int x1, int y1, int x2, int y2, int color);
void xw_raised(int x, int y, int w, int h);
void xw_sunken(int x, int y, int w, int h);
void xw_box(int x, int y, int w, int h, int color);
void xw_triangle(int x, int y, int size, int dir, int color);  /* dir 0 down, 1 up */
void xw_disc(int x, int y, int d, int color);                   /* filled circle, d wide */
void xw_circle(int x, int y, int d, int color);                 /* its outline */
void xw_polygon(const int *xy, int npoints, int color);         /* filled */
/* A picture given as rows of characters: '#' is drawn in `color`, '+' in
 * `color2` (-1: skipped), anything else is transparent. Each character is
 * drawn as a scale x scale square. */
void xw_picture(int x, int y, const char *const *rows, int scale, int color, int color2);
/* A full-color picture (a screenshot): `palette` holds 0xRRGGBB colors and
 * `rle` the runs over the w x h pixels, row after row, each a palette index
 * byte and then the run length less one in 7-bit groups, lowest first, the
 * top bit set on all but the last (see docs/guidepics.c). It is decoded
 * once into a server pixmap; returns a handle for xw_image_draw, or -1. */
int  xw_image_new(int w, int h, const unsigned long *palette, int ncolors,
                  const unsigned char *rle, size_t size);
/* Draw the w x h part of image `id` that starts at (sx, sy) at (x, y). */
void xw_image_draw(int id, int sx, int sy, int w, int h, int x, int y);

int  xw_text(int font, int x, int y, const char *s, int n, int color);
int  xw_text_width(int font, const char *s, int n);
/* Text `scale` times its size (blocky, like the pixel art), baseline at y;
 * returns its width. For the title on the splash screen. */
int  xw_text_scaled(int font, int x, int y, const char *s, int n, int scale, int color);
int  xw_ascent(int font);
int  xw_descent(int font);
static inline int xw_line_height(int font) { return xw_ascent(font) + xw_descent(font); }

void xw_clip(int x, int y, int w, int h);
void xw_clip_off(void);

/* ---- input ---- */
typedef struct {
    int x, y;
    int button;         /* 1 left, 2 middle, 3 right, 4/5 wheel */
    int clicks;         /* 1, or 2 for a double click */
} XwMouse;
extern XwMouse xw_mouse;

/* Block until a key, click, resize or close (keys.h). */
int  xw_next_key(void);
/* While xw_next_key waits, call fn every `ms` milliseconds (network play
 * keeps its connections and timers going under any dialog). If fn returns
 * nonzero, xw_next_key returns K_RESIZE so the caller redraws. NULL: no
 * idle function. fn is not called again while it runs. */
void xw_set_idle(int (*fn)(void), int ms);
/* The same, but give up after `ms` milliseconds with K_TIMEOUT. */
int  xw_wait_key(int ms);
/* Throw away pending keys and clicks (after a long computer turn). */
void xw_drain(void);

#endif
