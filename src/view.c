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

/* view.c - see view.h.
 *
 * WINDOW LAYOUT
 *   +--------------------------------------------------------------+
 *   | > Game   > Options   > Help                         menu bar  |
 *   +-------------------------------+------------------------------+
 *   |                               | players (one box each)       |
 *   |          the board            | status line                  |
 *   |      (square, as large        | [Roll] [Build] [Trade] [...] |
 *   |       as the window allows)   | record of events             |
 *   +-------------------------------+------------------------------+
 *
 * THE BOARD follows the 1904 patent drawing: MOTHER EARTH in one corner and
 * the circuit running from it; nine spaces on each side between the corners;
 * in the middle the four boxes of the patent ("Bank", "Wages", "Public
 * treasury", "Railroad") with the dice. Space 0 is the bottom-right corner
 * and play runs right to left along the bottom. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "view.h"
#include "app.h"
#include "art.h"
#include "menubar.h"
#include "ui.h"
#include "util.h"
#include "xwin.h"

#define GAP       8
#define PANEL_MIN 340

static char status_text[200];
static PanelButton focus = PB_NONE;
static int log_scroll;

/* Where things were last drawn, for the hit tests. */
static int bx0, by0, bsize, bcorner;
static UiRect buttons[PB_COUNT];

static const char *const button_labels[PB_COUNT] = { "&Roll Dice", "&Build...", "&Trade...", "Standings" };

void view_set_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(status_text, sizeof status_text, fmt, ap);
    va_end(ap);
}

void view_set_focus(PanelButton b) { focus = b; }

void view_scroll_log(int n)
{
    log_scroll = n ? MAX(0, log_scroll + n) : 0;
}

int app_player_color(int p) { return XC_PLAYER0 + (p & 3); }

/* ---- board geometry ---- */

/* The rectangle of space sp, relative to the board's top-left corner. */
static UiRect space_rect(int sp)
{
    int S = bsize, C = bcorner, inner = S - 2 * C;
    int side = sp / SIDE, k = sp % SIDE;
    int a = C + (k - 1) * inner / 9, b = C + k * inner / 9;    /* along the side, from its start */
    UiRect r = { 0, 0, C, C };
    switch (side) {
    case 0:     /* bottom row, right to left */
        if (k == 0) r = (UiRect){ S - C, S - C, C, C };
        else r = (UiRect){ S - b, S - C, b - a, C };
        break;
    case 1:     /* left column, bottom to top */
        if (k == 0) r = (UiRect){ 0, S - C, C, C };
        else r = (UiRect){ 0, S - b, C, b - a };
        break;
    case 2:     /* top row, left to right */
        if (k == 0) r = (UiRect){ 0, 0, C, C };
        else r = (UiRect){ a, 0, b - a, C };
        break;
    default:    /* right column, top to bottom */
        if (k == 0) r = (UiRect){ S - C, 0, C, C };
        else r = (UiRect){ S - C, a, C, b - a };
        break;
    }
    r.x += bx0;
    r.y += by0;
    return r;
}

int view_space_at(int x, int y)
{
    for (int i = 0; i < NSPACES; i++)
        if (ui_hit(space_rect(i), x, y)) return i;
    return -1;
}

PanelButton view_button_at(int x, int y)
{
    for (int i = 0; i < PB_COUNT; i++)
        if (ui_hit(buttons[i], x, y)) return (PanelButton)i;
    return PB_NONE;
}

/* ---- pieces of the board ---- */

static int space_color(int sp)
{
    switch (board[sp].type) {
    case T_LOT:         return XC_LOT;
    case T_TAX:         return XC_NECESSITY;
    case T_FRANCHISE:   return XC_FRANCHISE;
    case T_RAILROAD:    return XC_RAILROAD;
    case T_LUXURY:      return XC_LUXURY;
    case T_CHANCE:      return XC_CHANCE;
    case T_TRESPASS:    return XC_TRESPASS;
    case T_SPECULATION: return XC_FRANCHISE;
    case T_LEGACY:      return XC_LUXURY;
    default:            return XC_CORNER;
    }
}

static int space_art(int sp)
{
    switch (board[sp].type) {
    case T_MOTHER:      return ART_GLOBE;
    case T_JAIL:        return ART_JAIL;
    case T_PARK:        return ART_TREE;
    case T_POOR:        return ART_POORHOUSE;
    case T_RAILROAD:    return ART_TRAIN;
    case T_FRANCHISE:   return sp == 16 ? ART_BULB : ART_TROLLEY;
    case T_CHANCE:      return ART_CHANCE;
    case T_LUXURY:      return ART_DIAMOND;
    case T_TAX:         return sp == 2 ? ART_BREAD : sp == 18 ? ART_FLAME : sp == 11 ? ART_ROOF : ART_SHIRT;
    case T_TRESPASS:    return app.game.colleges[sp] ? ART_COLLEGE : ART_SIGN;
    case T_SPECULATION: return ART_DICE;
    case T_LEGACY:      return ART_SCROLL;
    default:            return -1;
    }
}

/* The second color of a picture ('+' pixels). */
static int art_color2(int art)
{
    switch (art) {
    case ART_GLOBE: case ART_TREE: return XC_LOT;
    case ART_BULB: case ART_FLAME: case ART_SCROLL: return XC_MARK;
    case ART_TRAIN: case ART_TROLLEY: return XC_TROUGH;
    case ART_DIAMOND: case ART_SHIRT: case ART_BREAD: case ART_ROOF: case ART_SIGN:
    case ART_DICE: case ART_COLLEGE:
        return XC_WHITE;
    default: return -1;
    }
}

/* Draw the lines of `label` ('\n' separated) centered in [x, x+w) from y.
 * Returns the y after the last line. */
static int centered_lines(int font, const char *label, int x, int w, int y, int color)
{
    int lh = xw_line_height(font);
    const char *p = label;
    while (*p) {
        const char *nl = strchr(p, '\n');
        int n = nl ? (int)(nl - p) : (int)strlen(p);
        int tw = xw_text_width(font, p, n);
        xw_text(font, x + (w - tw) / 2, y + xw_ascent(font), p, n, color);
        y += lh;
        p += n;
        if (*p == '\n') p++;
    }
    return y;
}

static int count_lines(const char *s)
{
    int n = 1;
    for (; *s; s++) if (*s == '\n') n++;
    return n;
}

static void draw_house(int x, int y, int scale, int color)
{
    xw_picture(x, y, art_rows(ART_HOUSE), scale, color, XC_INK);
}

/* Which edge of space sp faces the middle of the board (0 top, 1 right, 2
 * bottom, 3 left): houses and the owner's mark go there. */
static int inner_edge(int sp)
{
    switch (sp / SIDE) {
    case 0:  return 0;
    case 1:  return 1;
    case 2:  return 2;
    default: return 3;
    }
}

static void draw_space(int sp)
{
    const Game *g = &app.game;
    UiRect r = space_rect(sp);
    const Space *s = &board[sp];
    int corner = sp % SIDE == 0;
    xw_fill(r.x, r.y, r.w, r.h, space_color(sp));
    if (g->public_owned[sp]) xw_fill(r.x + 1, r.y + 1, r.w - 2, r.h - 2, XC_CORNER);

    int art = space_art(sp);
    int lh = xw_line_height(XF_SMALL), bh = xw_line_height(XF_BOLD);
    int nlines = count_lines(s->label);
    int has_price = board_has_card(sp) && !g->public_owned[sp];
    int has_rent = s->type == T_LOT;
    /* Price and rent on two lines when there is room, else on one. */
    int one_line = has_rent && nlines * lh + bh + lh > r.h - 4;
    int text_h = nlines * lh + (has_price ? bh : 0) + (has_rent && !one_line ? lh : 0);
    int scale = 1;
    if (corner) scale = MAX(1, bcorner / 50);
    else if (art >= 0 && text_h + 2 * art_height((ArtId)art) + 8 <= r.h && r.w >= 40) scale = 2;
    int ah = art >= 0 ? art_height((ArtId)art) * scale : 0;
    int aw = art >= 0 ? art_width((ArtId)art) * scale : 0;
    int show_art = art >= 0 && text_h + ah + 4 <= r.h - 4;
    /* No room for both: a space with a name of its own keeps the name, the
     * others (Chance, Luxury, the necessities) keep their picture. */
    if (art >= 0 && !show_art && s->type != T_TRESPASS && !board_has_card(sp) && ah + 4 <= r.h) {
        show_art = 1;
        nlines = 0;
        text_h = 0;
    }
    int total = (show_art ? ah + 3 : 0) + text_h;
    int y = r.y + MAX(2, (r.h - total) / 2);
    if (corner) y = r.y + 4;            /* the checkers stand at the bottom of a corner */
    xw_clip(r.x + 1, r.y + 1, r.w - 2, r.h - 2);
    if (show_art) {
        xw_picture(r.x + (r.w - aw) / 2, y, art_rows((ArtId)art), scale, XC_INK, art_color2(art));
        y += ah + 3;
    }
    if (nlines) y = centered_lines(XF_SMALL, s->label, r.x, r.w, y, XC_INK);
    if (has_price && one_line) {
        char p1[16], p2[24];
        snprintf(p1, sizeof p1, "$%d", s->price);
        snprintf(p2, sizeof p2, "  rent %d", game_land_rent(g, sp));
        int w1 = xw_text_width(XF_BOLD, p1, (int)strlen(p1)), w2 = xw_text_width(XF_SMALL, p2, (int)strlen(p2));
        int x0 = r.x + (r.w - w1 - w2) / 2;
        xw_text(XF_BOLD, x0, y + xw_ascent(XF_BOLD), p1, (int)strlen(p1), XC_INK);
        xw_text(XF_SMALL, x0 + w1, y + xw_ascent(XF_BOLD), p2, (int)strlen(p2), XC_INK);
    } else {
        if (has_price) {
            char buf[24];
            snprintf(buf, sizeof buf, "$%d", s->price);
            y = centered_lines(XF_BOLD, buf, r.x, r.w, y, XC_INK);
        }
        if (has_rent) {
            char buf[24];
            snprintf(buf, sizeof buf, "rent %d", game_land_rent(g, sp));
            centered_lines(XF_SMALL, buf, r.x, r.w, y, XC_INK);
        }
    }

    /* The owner's mark (a square of his color) and the houses, along the
     * edge that faces the middle of the board. */
    int edge = inner_edge(sp);
    int o = app.playing ? g->owner[sp] : NOBODY;
    if (o != NOBODY) {
        int m = 9, mx, my;
        switch (edge) {
        case 0:  mx = r.x + 3;          my = r.y + 3; break;
        case 1:  mx = r.x + r.w - m - 3; my = r.y + 3; break;
        case 2:  mx = r.x + 3;          my = r.y + r.h - m - 3; break;
        default: mx = r.x + 3;          my = r.y + 3; break;
        }
        xw_fill(mx, my, m, m, app_player_color(o));
        xw_box(mx, my, m, m, XC_INK);
    }
    int nh = app.playing ? g->houses[sp] : 0;
    if (nh) {
        int hs = r.w >= 52 || r.h >= 52 ? 2 : 1;
        int hw = 7 * hs, hh = 6 * hs;
        int col = app_player_color(g->house_owner[sp] >= 0 ? g->house_owner[sp] : 0);
        int shown = MIN(nh, 3);
        for (int i = 0; i < shown; i++) {
            int hx, hy;
            if (edge == 0 || edge == 2) {
                hx = r.x + r.w - (i + 1) * (hw + 2) - 2;
                hy = edge == 0 ? r.y + 3 : r.y + r.h - hh - 3;
            } else {
                hx = edge == 1 ? r.x + r.w - hw - 3 : r.x + 3;
                hy = r.y + r.h - (i + 1) * (hh + 2) - 2;
            }
            draw_house(hx, hy, hs, col);
        }
        if (nh > 3) {
            char buf[16];
            snprintf(buf, sizeof buf, "x%d", nh);
            xw_text(XF_SMALL, r.x + 3, r.y + r.h - 3, buf, (int)strlen(buf), XC_INK);
        }
    }
    xw_clip_off();
    xw_box(r.x, r.y, r.w + (r.x + r.w < bx0 + bsize), r.h + (r.y + r.h < by0 + bsize), XC_INK);
    if (app.highlight == sp) {
        xw_box(r.x + 2, r.y + 2, r.w - 3, r.h - 3, XC_INK);
        xw_box(r.x + 3, r.y + 3, r.w - 5, r.h - 5, XC_INK);
    }
}

static void draw_tokens(void)
{
    const Game *g = &app.game;
    for (int sp = 0; sp < NSPACES; sp++) {
        int here[MAXPLAYERS], n = 0;
        for (int p = 0; p < g->nplayers; p++) if (g->pl[p].pos == sp) here[n++] = p;
        if (!n) continue;
        UiRect r = space_rect(sp);
        int d = CLAMP(MIN(r.w, r.h) / 4, 10, 18);
        /* Up to four checkers in a 2 x 2 group in the middle of the space. */
        int gw = n > 1 ? 2 * d + 2 : d, gh = n > 2 ? 2 * d + 2 : d;
        int x0 = r.x + (r.w - gw) / 2, y0 = r.y + (r.h - gh) / 2;
        if (sp % SIDE == 0) y0 = r.y + r.h - gh - 4;        /* corners: keep the picture visible */
        for (int i = 0; i < n; i++) {
            int p = here[i];
            int x = x0 + (i % 2) * (d + 2), y = y0 + (i / 2) * (d + 2);
            xw_disc(x + 1, y + 2, d, XC_TROUGH);            /* a shadow */
            xw_disc(x, y, d, app_player_color(p));
            xw_circle(x, y, d, XC_INK);
            if (g->pl[p].in_jail) {                          /* behind bars */
                xw_vline(x + d / 3, y + 1, d - 2, XC_INK);
                xw_vline(x + 2 * d / 3, y + 1, d - 2, XC_INK);
            }
            if (p == g->cur && !g->over) xw_disc(x + d / 2 - 2, y + d / 2 - 2, 4, XC_WHITE);
        }
    }
}

/* A die, s pixels square, showing v (0 = blank). */
static void draw_die(int x, int y, int s, int v)
{
    xw_fill(x + 2, y + 2, s, s, XC_TROUGH);
    xw_fill(x, y, s, s, XC_WHITE);
    xw_box(x, y, s, s, XC_INK);
    static const char pips[7][9] = {
        "         ", "    #    ", "#       #", "#   #   #",
        "# #   # #", "# # # # #", "# ## ## #",
    };
    if (v < 1 || v > 6) return;
    int d = MAX(4, s / 6), step = s / 4;
    for (int i = 0; i < 9; i++) {
        if (pips[v][i] != '#') continue;
        int cx = x + step * (1 + i % 3), cy = y + step * (1 + i / 3);
        xw_disc(cx - d / 2, cy - d / 2, d, XC_INK);
    }
}

/* One of the patent's boxes in the middle of the board. */
static void center_box(int x, int y, int w, int h, const char *title, const char *value)
{
    xw_fill(x, y, w, h, XC_CORNER);
    xw_box(x, y, w, h, XC_INK);
    xw_box(x + 2, y + 2, w - 4, h - 4, XC_INK);
    int lh = xw_line_height(XF_SMALL);
    int ty = y + (h - lh - xw_line_height(XF_BOLD)) / 2;
    centered_lines(XF_SMALL, title, x, w, ty, XC_INK);
    centered_lines(XF_BOLD, value, x, w, ty + lh, XC_INK);
}

static void draw_center(void)
{
    const Game *g = &app.game;
    int C = bcorner, x = bx0 + C, y = by0 + C, w = bsize - 2 * C;
    xw_fill(x + 1, y + 1, w - 1, w - 1, XC_CORNER);

    int cy = y + w / 10;
    const char *t1 = "THE LANDLORD'S GAME";
    int tw = xw_text_width(XF_LOGO, t1, (int)strlen(t1));
    xw_text(XF_LOGO, x + (w - tw) / 2, cy + xw_ascent(XF_LOGO), t1, (int)strlen(t1), XC_INK);
    cy += xw_line_height(XF_LOGO) + 2;
    cy = centered_lines(XF_SMALL, "after Lizzie J. Magie  -  Patented Jan. 5, 1904", x, w, cy, XC_INK);
    cy = centered_lines(XF_SMALL, g->single_tax ? "THE SINGLE TAX IS IN FORCE"
                                                : "Rules of the Economic Game Co., New York, 1906",
                        x, w, cy + 2, XC_INK);
    cy = centered_lines(XF_BOLD, "LABOR UPON MOTHER EARTH PRODUCES WAGES", x, w, cy + 4, XC_INK);

    /* The four boxes of the patent, two by two, around the dice. */
    int bw = w * 30 / 100, bh = MAX(44, w / 9);
    int gap = w / 20;
    int left = x + (w - 2 * bw - gap) / 2;
    int top = y + w * 30 / 100;
    char v[4][48];
    snprintf(v[0], sizeof v[0], "$%d", g->treasury);
    snprintf(v[1], sizeof v[1], "$%d", g->wage);
    if (g->trust_a >= 0) snprintf(v[2], sizeof v[2], "Trust: $40");
    else snprintf(v[2], sizeof v[2], "$5 / Pass on doubles");
    int nh = game_houses_total(g);
    snprintf(v[3], sizeof v[3], "%d house%s", nh, nh == 1 ? "" : "s");
    center_box(left, top, bw, bh, "PUBLIC TREASURY", v[0]);
    center_box(left + bw + gap, top, bw, bh, "WAGES", v[1]);
    int bot = top + bh + gap + MAX(48, w / 7) + gap;
    center_box(left, bot, bw, bh, "RAILROAD", v[2]);
    center_box(left + bw + gap, bot, bw, bh, "MISCELLANEOUS", v[3]);

    /* The dice between the boxes. */
    int ds = CLAMP(w / 9, 28, 54);
    int dy = top + bh + gap + (MAX(48, w / 7) - ds) / 2;
    int dx = x + (w - 2 * ds - 14) / 2;
    int d1 = g->last_d1, d2 = g->last_d2;
    if (app.dice_rolling) { d1 = 1 + (int)((unsigned)app.dice_rolling * 7u % 6u); d2 = 1 + (int)((unsigned)app.dice_rolling * 5u % 6u); }
    draw_die(dx, dy, ds, d1);
    draw_die(dx + ds + 14, dy, ds, d2);

    /* The newest event under the boxes. */
    const char *last = game_log_line(g, 0);
    if (last && app.playing) {
        char buf[400];
        const char *lines[4];
        int n = ui_wrap(XF_UI, last, w - 30, buf, sizeof buf, lines, 3);
        int ly = bot + bh + MAX(10, gap);
        for (int i = 0; i < n; i++) {
            int lw = xw_text_width(XF_UI, lines[i], (int)strlen(lines[i]));
            xw_text(XF_UI, x + (w - lw) / 2, ly + xw_ascent(XF_UI), lines[i], (int)strlen(lines[i]), XC_INK);
            ly += xw_line_height(XF_UI);
        }
    }
}

static void draw_board(int x, int y, int S)
{
    bx0 = x;
    by0 = y;
    bsize = S;
    bcorner = S * 135 / 1000;
    xw_fill(x - 4, y - 4, S + 8, S + 8, XC_SURFACE);
    xw_raised(x - 4, y - 4, S + 8, S + 8);
    xw_fill(x, y, S, S, XC_CORNER);
    if (app.playing) draw_center();
    for (int i = 0; i < NSPACES; i++) draw_space(i);
    xw_box(x, y, S, S, XC_INK);
    xw_box(x + bcorner, y + bcorner, S - 2 * bcorner + 1, S - 2 * bcorner + 1, XC_INK);
    if (app.playing) draw_tokens();
    else {
        int w = S - 2 * bcorner;
        int cy = y + bcorner + w / 3;
        const char *t1 = "THE LANDLORD'S GAME";
        int tw = xw_text_width(XF_LOGO, t1, (int)strlen(t1));
        xw_text(XF_LOGO, x + (S - tw) / 2, cy, t1, (int)strlen(t1), XC_INK);
        centered_lines(XF_UI, "Viola the Landlord\n\nGame > New Game (Ctrl+N) to begin.",
                       x, S, cy + 16, XC_INK);
    }
}

/* ---- the panel ---- */

static const char *const diff_names[] = { "Easy", "Medium", "Hard" };

static int draw_player(int p, int x, int y, int w)
{
    const Game *g = &app.game;
    const Player *P = &g->pl[p];
    int lh = xw_line_height(XF_UI), sh = xw_line_height(XF_FIXED);
    int h = lh + 2 * sh + 14;
    int cur = p == g->cur && !g->over;
    xw_fill(x, y, w, h, cur ? XC_MARK : XC_SURFACE);
    xw_sunken(x, y, w, h);
    if (cur) xw_box(x + 3, y + 3, w - 6, h - 6, XC_INK);
    int d = lh - 4;
    xw_disc(x + 9, y + 7, d, app_player_color(p));
    xw_circle(x + 9, y + 7, d, XC_INK);
    char buf[160];
    snprintf(buf, sizeof buf, "%s%s%s%s", P->name, P->cpu ? " (" : "", P->cpu ? diff_names[CLAMP(P->difficulty, 0, 2)] : "",
             P->cpu ? ")" : "");
    xw_clip(x + 4, y + 4, w - 8, h - 8);
    xw_text(XF_UI, x + 15 + d, y + 6 + xw_ascent(XF_UI), buf, (int)strlen(buf), XC_INK);
    snprintf(buf, sizeof buf, "$%d", P->cash);
    int cw = xw_text_width(XF_UI, buf, (int)strlen(buf));
    xw_text(XF_UI, x + w - 10 - cw, y + 6 + xw_ascent(XF_UI), buf, (int)strlen(buf), XC_INK);
    int ly = y + 8 + lh + xw_ascent(XF_FIXED);
    snprintf(buf, sizeof buf, "Worth $%-5d Wages %d/%d  Cards %d  Houses %d", game_worth(g, p), P->wages,
             g->end_wages, game_cards_of(g, p, -1), game_houses_of(g, p));
    xw_text(XF_FIXED, x + 10, ly, buf, (int)strlen(buf), XC_INK);
    buf[0] = '\0';
    if (P->in_jail) strcat(buf, "IN JAIL   ");
    if (P->in_poorhouse) strcat(buf, "IN THE POOR HOUSE   ");
    if (P->natural_opp) strcat(buf, "ON A NATURAL OPPORTUNITY   ");
    char more[80];
    snprintf(more, sizeof more, "At %s", board[P->pos].name);
    strcat(buf, more);
    if (P->luxuries) { snprintf(more, sizeof more, "  Luxuries %d", P->luxuries); strcat(buf, more); }
    if (P->education || P->professors) {
        snprintf(more, sizeof more, "  Educ. %d Prof. %d", P->education, P->professors);
        strcat(buf, more);
    }
    xw_text(XF_FIXED, x + 10, ly + sh, buf, (int)strlen(buf), XC_INK);
    xw_clip_off();
    return h;
}

static void draw_log(int x, int y, int w, int h)
{
    const Game *g = &app.game;
    xw_fill(x, y, w, h, XC_SURFACE);
    xw_sunken(x, y, w, h);
    int lh = xw_line_height(XF_FIXED);
    int rows = (h - 8) / lh;
    if (rows <= 0) return;
    /* Lay the newest messages out bottom-up, wrapping long ones. */
    xw_clip(x + 3, y + 3, w - 6, h - 6);
    int maxw = w - 16;
    int yy = y + h - 5 - xw_descent(XF_FIXED);
    int skip = log_scroll;
    for (int i = 0; yy > y + 4; i++) {
        const char *line = game_log_line(g, i);
        if (!line) break;
        char buf[400];
        const char *parts[6];
        int n = ui_wrap(XF_FIXED, line, maxw, buf, sizeof buf, parts, 6);
        for (int k = n - 1; k >= 0 && yy > y + 4; k--) {
            if (skip > 0) { skip--; continue; }
            int indent = k > 0 ? 12 : 0;
            xw_text(XF_FIXED, x + 8 + indent, yy, parts[k], (int)strlen(parts[k]), XC_INK);
            yy -= lh;
        }
    }
    xw_clip_off();
    if (log_scroll) {
        const char *m = "(scrolled: End for the newest)";
        int mw = xw_text_width(XF_FIXED, m, (int)strlen(m));
        xw_fill(x + w - mw - 12, y + 4, mw + 6, lh, XC_MARK);
        xw_text(XF_FIXED, x + w - mw - 9, y + 4 + xw_ascent(XF_FIXED), m, (int)strlen(m), XC_INK);
    }
}

static void draw_panel(int x, int y, int w, int h)
{
    const Game *g = &app.game;
    int yy = y;
    if (app.playing) {
        for (int p = 0; p < g->nplayers; p++) yy += draw_player(p, x, yy, w) + 6;
    }
    /* The status line. */
    int lh = xw_line_height(XF_UI);
    xw_clip(x, yy, w, lh + 8);
    xw_text(XF_UI, x + 2, yy + 4 + xw_ascent(XF_UI), status_text, (int)strlen(status_text), XC_INK);
    xw_clip_off();
    yy += lh + 10;
    /* The buttons. */
    int bw = (w - 3 * 8) / 4;
    for (int i = 0; i < PB_COUNT; i++) {
        buttons[i] = (UiRect){ x + i * (bw + 8), yy, bw, UI_BUTTON_H };
        int enabled = i == PB_SCORES ? app.playing : app_can_act();
        ui_button(buttons[i], button_labels[i], enabled && focus == i);
        if (!enabled) {                     /* grayed: redraw the label in trough gray */
            char shown[32];
            const char *s = button_labels[i];
            int n = 0;
            for (; *s && n < 31; s++) if (*s != '&') shown[n++] = *s;
            shown[n] = '\0';
            int tw = xw_text_width(XF_UI, shown, n);
            int by = buttons[i].y + (UI_BUTTON_H - lh) / 2 + xw_ascent(XF_UI);
            xw_fill(buttons[i].x + 3, buttons[i].y + 3, bw - 6, UI_BUTTON_H - 6, XC_SURFACE);
            xw_text(XF_UI, buttons[i].x + (bw - tw) / 2, by, shown, n, XC_TROUGH);
        }
    }
    yy += UI_BUTTON_H + 10;
    /* The record of events. */
    xw_text(XF_UI, x + 2, yy + xw_ascent(XF_UI), "Events", 6, XC_INK);
    yy += lh + 4;
    draw_log(x, yy, w, y + h - yy);
}

void view_draw(void)
{
    int W = xw_width(), H = xw_height();
    xw_fill(0, 0, W, H, XC_SURFACE);
    int top = 4 + menubar_height() + GAP + 4;
    int S = MIN(H - top - GAP - 4, W - PANEL_MIN - 3 * GAP - 8);
    S = MAX(S, 300);
    draw_board(GAP + 4, top, S);
    int px = GAP + 4 + S + 4 + 2 * GAP;
    draw_panel(px, top - 4, W - px - GAP, H - top - GAP + 4);
    menubar_draw(-1);
}
