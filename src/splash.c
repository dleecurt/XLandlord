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

/* splash.c - see splash.h.
 *
 * THE TITLE CARD fills the window (menu bar included):
 *
 *   +--------------------------------------------------------------+
 *   |  +--------------------------------------------------------+  |
 *   |  |        (globe)  (house)  (train)  (tree)  (dice)        |  |
 *   |  |                    X L a n d l o r d                    |  |   <- XF_LOGO, scaled up
 *   |  |                  THE LANDLORD'S GAME                    |  |
 *   |  |   invented by Lizzie J. Magie - US Patent 748,626, 1904 |  |
 *   |  |   the rules of 1906, with the Single Tax                |  |
 *   |  |        LABOR UPON MOTHER EARTH PRODUCES WAGES           |  |
 *   |  |   [Host a Network Game] [Join ...] [Play Here] [Quit]   |  |   <- or "Starting..."
 *   |  |   (a line saying what the focused choice does)          |  |
 *   |  |        version, copyright, license                      |  |
 *   |  +--------------------------------------------------------+  |
 *   +--------------------------------------------------------------+
 *
 * The card is the board's cream with the board's double rule round it, on
 * Viola's gray94; the buttons are ordinary Viola buttons. */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "splash.h"
#include "art.h"
#include "keys.h"
#include "ui.h"
#include "util.h"
#include "version.h"
#include "xwin.h"

static const char *const choice_label[SPLASH_COUNT] = {
    "&Host a Network Game", "&Join a Network Game", "&Play Here", "&Quit",
};
static const char *const choice_note[SPLASH_COUNT] = {
    "Run the game on this machine; up to three players join from theirs, over ssh.",
    "Play in a game someone else is hosting, by its IP address or host name.",
    "Play at this screen, against the computer or with others taking turns.",
    "Leave XLandlord.",
};

/* Centered text on one line at baseline y in the card [x, x+w). */
static void center(int font, const char *s, int x, int w, int y, int color)
{
    int n = (int)strlen(s), tw = xw_text_width(font, s, n);
    xw_text(font, x + (w - tw) / 2, y, s, n, color);
}

/* Draw the card; with `choices`, the buttons (focus on `focus`) and what
 * the focused one does, else `note` where the buttons would be. Fills in
 * the buttons' places. */
static void draw(int choices, int focus, const char *note, UiRect *b)
{
    int W = xw_width(), H = xw_height();
    xw_fill(0, 0, W, H, XC_SURFACE);
    /* The card: cream, a double rule round it, as on the board. */
    int m = MAX(12, MIN(W, H) / 30);
    int cx = m, cy = m, cw = W - 2 * m, ch = H - 2 * m;
    xw_fill(cx, cy, cw, ch, XC_CORNER);
    xw_box(cx, cy, cw, ch, XC_INK);
    xw_box(cx + 4, cy + 4, cw - 8, ch - 8, XC_INK);

    /* Measure the column, then centre it in the card. */
    int scale = CLAMP(MIN(cw / 360, ch / 260), 2, 4);      /* the title's blocks */
    int art_scale = scale + 1;
    static const ArtId row[] = { ART_GLOBE, ART_POORHOUSE, ART_TRAIN, ART_TREE, ART_DICE };
    int nart = (int)(sizeof row / sizeof row[0]);
    int art_h = 0, art_w = 0;
    for (int i = 0; i < nart; i++) {
        art_h = MAX(art_h, art_height(row[i]) * art_scale);
        art_w += art_width(row[i]) * art_scale + (i ? 28 : 0);
    }
    int lh = xw_line_height(XF_UI), lsmall = xw_line_height(XF_SMALL);
    int title_h = xw_line_height(XF_LOGO) * scale;
    int total = art_h + 24 + title_h + 10 + xw_line_height(XF_LOGO) + 14 + 2 * lh + 12
                + xw_line_height(XF_BOLD) + 30 + UI_BUTTON_H + 10 + lh + 26 + 2 * lsmall;
    int y = cy + MAX(16, (ch - total) / 2);

    /* The pictures, from the board. */
    int x = cx + (cw - art_w) / 2;
    for (int i = 0; i < nart; i++) {
        int ah = art_height(row[i]) * art_scale;
        int c2 = row[i] == ART_GLOBE || row[i] == ART_TREE ? XC_LOT : row[i] == ART_DICE ? XC_WHITE
                 : row[i] == ART_TRAIN ? XC_TROUGH : -1;
        xw_picture(x, y + (art_h - ah), art_rows(row[i]), art_scale, XC_INK, c2);
        x += art_width(row[i]) * art_scale + 28;
    }
    y += art_h + 24;

    /* XLandlord, large. */
    const char *name = "XLandlord";
    int nw = xw_text_width(XF_LOGO, name, (int)strlen(name)) * scale;
    y += xw_ascent(XF_LOGO) * scale;
    xw_text_scaled(XF_LOGO, cx + (cw - nw) / 2, y, name, (int)strlen(name), scale, XC_INK);
    y += xw_descent(XF_LOGO) * scale + 10;

    y += xw_ascent(XF_LOGO);
    center(XF_LOGO, "THE LANDLORD'S GAME", cx, cw, y, XC_INK);
    y += xw_descent(XF_LOGO) + 14;
    y += xw_ascent(XF_UI);
    center(XF_UI, "invented by Lizzie J. Magie  -  US Patent 748,626, January 5, 1904", cx, cw, y, XC_INK);
    y += lh;
    center(XF_UI, "played by the rules of the Economic Game Co., New York, 1906, with the Single Tax",
           cx, cw, y, XC_INK);
    y += xw_descent(XF_UI) + 12 + xw_ascent(XF_BOLD);
    center(XF_BOLD, "LABOR UPON MOTHER EARTH PRODUCES WAGES", cx, cw, y, XC_INK);
    y += xw_descent(XF_BOLD) + 30;

    /* The first question, or a note while starting. */
    if (choices) {
        int bw[SPLASH_COUNT], row_w = 0;
        for (int i = 0; i < SPLASH_COUNT; i++) {
            char shown[64];
            int o = 0;
            for (const char *p = choice_label[i]; *p && o < 63; p++) if (*p != '&') shown[o++] = *p;
            shown[o] = '\0';
            bw[i] = MAX(UI_BUTTON_W, xw_text_width(XF_UI, shown, o) + 32);
            row_w += bw[i] + (i ? 12 : 0);
        }
        int bx = cx + (cw - row_w) / 2;
        for (int i = 0; i < SPLASH_COUNT; i++) {
            b[i] = (UiRect){ bx, y, bw[i], UI_BUTTON_H };
            ui_button(b[i], choice_label[i], i == focus);
            bx += bw[i] + 12;
        }
        y += UI_BUTTON_H + 10 + xw_ascent(XF_UI);
        center(XF_UI, choice_note[focus], cx, cw, y, XC_INK);
    } else {
        y += xw_ascent(XF_UI) + (UI_BUTTON_H - lh) / 2;
        center(XF_UI, note, cx, cw, y, XC_INK);
        y += UI_BUTTON_H - lh;
    }
    y += xw_descent(XF_UI) + 26;

    /* The small print at the foot. */
    char line[200];
    snprintf(line, sizeof line, "XLandlord %s  -  Copyright (C) 2026 Lee Curtis", PACKAGE_VERSION_STR);
    y += xw_ascent(XF_SMALL);
    center(XF_SMALL, line, cx, cw, y, XC_INK);
    y += lsmall;
    center(XF_SMALL, "Free software under the GNU GPL, version 3 or later. The board follows the public-domain "
           "1904 patent drawing.", cx, cw, y, XC_INK);
    xw_flush();
}

/* See splash.h. */
SplashChoice splash_ask(void)
{
    int focus = SPLASH_LOCAL;
    UiRect b[SPLASH_COUNT];
    for (;;) {
        draw(1, focus, NULL, b);
        int k = xw_next_key();
        if (k == K_RESIZE) continue;
        if (k == K_CLOSE) return SPLASH_QUIT;
        if (k == K_MOUSE) {
            if (xw_mouse.button != 1) continue;
            for (int i = 0; i < SPLASH_COUNT; i++) if (ui_hit(b[i], xw_mouse.x, xw_mouse.y)) return (SplashChoice)i;
            continue;
        }
        if (k == '\n' || k == '\r' || k == ' ') return (SplashChoice)focus;
        if (k == 27) { focus = SPLASH_QUIT; continue; }
        if (k == K_LEFT || k == K_BTAB) { focus = (focus + SPLASH_COUNT - 1) % SPLASH_COUNT; continue; }
        if (k == K_RIGHT || k == '\t') { focus = (focus + 1) % SPLASH_COUNT; continue; }
        if (k > 0 && k < 128) {
            int c = tolower(k);
            if (c == 'h') return SPLASH_HOST;
            if (c == 'j') return SPLASH_JOIN;
            if (c == 'p') return SPLASH_LOCAL;
            if (c == 'q') return SPLASH_QUIT;
        }
    }
}

/* See splash.h. */
void splash_show(const char *note, int ms)
{
    UiRect b[SPLASH_COUNT];
    draw(0, 0, note ? note : "", b);
    for (int left = ms; left > 0; left -= 100) {
        int k = xw_wait_key(100);
        if (k == K_RESIZE) { draw(0, 0, note ? note : "", b); continue; }
        if (k != K_TIMEOUT) return;             /* any key or click goes on at once */
    }
}
