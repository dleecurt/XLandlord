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

/* menubar.c - see menubar.h.
 *
 * THE LOOK (the Viola design system, as in Ted Viola): menu titles are plain
 * Helvetica words with Viola's small triangle in front and no border; while
 * the bar is in use the marked title gets a 1px black rectangle. A drop-down
 * is a raised gray94 panel with a trough-gray outline; its selected item is
 * marked by a 1px black rectangle, never a fill. */
#include <ctype.h>
#include <string.h>

#include "menubar.h"
#include "app.h"
#include "keys.h"
#include "util.h"
#include "xwin.h"

#define MAX_ITEMS 12
#define PAD 4

typedef struct {
    const char *title;
    MenuItem items[MAX_ITEMS];
} Menu;

static const Menu menus[NMENUS] = {
    [MENU_GAME] = { "Game", {
        { "&New Game...",              "Ctrl+N", CMD_NEW_GAME,   NULL, NULL },
        { "&Roll the Dice",            "Space",  CMD_ROLL,       NULL, app_can_act },
        { "&Build a House...",         "B",      CMD_BUILD,      NULL, app_can_act },
        { "Buy a &Card from a Player...", "T",   CMD_TRADE_BUY,  NULL, app_can_act },
        { "Sell a Card to a &Player...", "S",    CMD_TRADE_SELL, NULL, app_can_act },
        { "Sell to the Pac&k...",      NULL,     CMD_SELL_PACK,  NULL, app_can_act },
        { "Propose the Single &Tax...", "X",     CMD_SINGLE_TAX, NULL, app_can_propose },
        { "&Standings",                "F2",     CMD_SCORES,     NULL, NULL },
        { "&Quit",                     "Ctrl+Q", CMD_QUIT,       NULL, NULL } } },
    [MENU_OPTIONS] = { "Options", {
        { "&Game Settings...",         NULL,     CMD_SETTINGS,   NULL, NULL },
        { "Computer &Speed...",        NULL,     CMD_SPEED,      NULL, NULL },
        { "Ask Me to &Bid",            NULL,     CMD_TOGGLE_BIDS,    app_bids_on, NULL },
        { "&Confirm Each Computer Move", NULL,   CMD_TOGGLE_CONFIRM, app_confirm_on, NULL } } },
    [MENU_HELP] = { "Help", {
        { "&How to Play",              "F1",     CMD_HELP_PLAY,    NULL, NULL },
        { "The &Rules of 1906",        NULL,     CMD_HELP_RULES,   NULL, NULL },
        { "&Lizzie Magie and the Game", NULL,    CMD_HELP_HISTORY, NULL, NULL },
        { "&Keyboard",                 NULL,     CMD_HELP_KEYS,    NULL, NULL },
        { "Li&cense",                  NULL,     CMD_LICENSE,      NULL, NULL },
        { "&About",                    NULL,     CMD_ABOUT,        NULL, NULL } } },
};

static int item_count(const Menu *m)
{
    int c = 0;
    while (c < MAX_ITEMS && m->items[c].label) c++;
    return c;
}

static int split_label(const char *label, char *shown, size_t size)
{
    int pos = -1;
    size_t o = 0;
    for (const char *p = label; *p && o + 1 < size; p++) {
        if (*p == '&' && p[1]) { pos = (int)o; continue; }
        shown[o++] = *p;
    }
    shown[o] = '\0';
    return pos;
}

static int item_enabled(const MenuItem *it) { return !it->enabled || it->enabled(); }

/* ---- geometry ---- */

#define TITLE_PAD   3
#define TRIANGLE    8
#define LABEL_GAP   6
#define TITLE_GAP   18

int menubar_height(void) { return xw_line_height(XF_UI) + 12; }

static int title_width(int i)
{
    const char *t = menus[i].title;
    return TITLE_PAD + TRIANGLE + LABEL_GAP + xw_text_width(XF_UI, t, (int)strlen(t)) + TITLE_PAD;
}

static int title_x(int i)
{
    int x = PAD + 6;
    for (int j = 0; j < i; j++) x += title_width(j) + TITLE_GAP;
    return x;
}

int menubar_hit(int x, int y)
{
    if (y < PAD || y >= PAD + menubar_height()) return -1;
    for (int i = 0; i < NMENUS; i++) {
        int tx = title_x(i);
        if (x >= tx - TITLE_GAP / 2 && x < tx + title_width(i) + TITLE_GAP / 2) return i;
    }
    return -1;
}

int menubar_for_letter(int c)
{
    for (int i = 0; i < NMENUS; i++)
        if (tolower((unsigned char)menus[i].title[0]) == tolower(c)) return i;
    return -1;
}

static void underline(int font, int x, int y, const char *s, int pos, int color)
{
    xw_hline(x + xw_text_width(font, s, pos), y + 2, xw_text_width(font, s + pos, 1), color);
}

void menubar_draw(int active)
{
    int x = PAD, y = PAD, w = xw_width() - 2 * PAD, h = menubar_height();
    xw_fill(x, y, w, h, XC_SURFACE);
    xw_raised(x, y, w, h);
    int by = y + (h - xw_line_height(XF_UI)) / 2 + xw_ascent(XF_UI);
    for (int i = 0; i < NMENUS; i++) {
        int tx = title_x(i);
        const char *t = menus[i].title;
        xw_triangle(tx + TITLE_PAD, y + (h - TRIANGLE) / 2, TRIANGLE, 0, XC_INK);
        int lx = tx + TITLE_PAD + TRIANGLE + LABEL_GAP;
        xw_text(XF_UI, lx, by, t, (int)strlen(t), XC_INK);
        if (active >= 0) underline(XF_UI, lx, by, t, 0, XC_INK);
        if (i == active) xw_box(tx - 2, y + 4, title_width(i) + 4, h - 8, XC_INK);
    }
}

typedef struct {
    int x, y, w, h, item_h, cnt;
    char shown[MAX_ITEMS][48];
    int hotpos[MAX_ITEMS];
} Dropdown;

#define DD_INSET 4
#define DD_CHECK 18

static void measure(int m, Dropdown *d)
{
    const Menu *mn = &menus[m];
    int lw = 0, hw = 0;
    d->cnt = item_count(mn);
    for (int i = 0; i < d->cnt; i++) {
        const MenuItem *it = &mn->items[i];
        d->hotpos[i] = split_label(it->label, d->shown[i], sizeof d->shown[i]);
        lw = MAX(lw, xw_text_width(XF_UI, d->shown[i], (int)strlen(d->shown[i])));
        if (it->hint) hw = MAX(hw, xw_text_width(XF_UI, it->hint, (int)strlen(it->hint)));
    }
    d->item_h = xw_line_height(XF_UI) + 6;
    d->w = DD_INSET + DD_CHECK + lw + (hw ? 28 + hw : 0) + 12 + DD_INSET;
    d->h = 2 * DD_INSET + d->cnt * d->item_h;
    d->x = MAX(0, MIN(title_x(m) - 2, xw_width() - d->w - PAD));
    d->y = PAD + menubar_height();
}

static void draw_dropdown(int m, const Dropdown *d, int sel)
{
    const Menu *mn = &menus[m];
    xw_fill(d->x, d->y, d->w, d->h, XC_SURFACE);
    xw_box(d->x, d->y, d->w, d->h, XC_TROUGH);
    xw_raised(d->x + 1, d->y + 1, d->w - 2, d->h - 2);
    for (int i = 0; i < d->cnt; i++) {
        const MenuItem *it = &mn->items[i];
        int iy = d->y + DD_INSET + i * d->item_h;
        int by = iy + (d->item_h - xw_line_height(XF_UI)) / 2 + xw_ascent(XF_UI);
        int lx = d->x + DD_INSET + DD_CHECK;
        int ink = item_enabled(it) ? XC_INK : XC_TROUGH;   /* disabled items are grayed */
        if (i == sel) xw_box(d->x + DD_INSET - 1, iy, d->w - 2 * DD_INSET + 2, d->item_h, XC_INK);
        if (it->checked && it->checked())
            xw_fill(d->x + DD_INSET + 6, iy + (d->item_h - 6) / 2, 6, 6, XC_INK);
        xw_text(XF_UI, lx, by, d->shown[i], (int)strlen(d->shown[i]), ink);
        if (d->hotpos[i] >= 0) underline(XF_UI, lx, by, d->shown[i], d->hotpos[i], ink);
        if (it->hint) {
            int hn = (int)strlen(it->hint), hw = xw_text_width(XF_UI, it->hint, hn);
            xw_text(XF_UI, d->x + d->w - DD_INSET - 12 - hw, by, it->hint, hn, ink);
        }
    }
}

static Command choose(const MenuItem *it)
{
    if (!item_enabled(it)) { xw_bell(); return CMD_NONE; }
    return it->cmd;
}

Command menubar_run(int m, int open, void (*redraw)(void))
{
    int sel = 0;
    for (;;) {
        redraw();
        menubar_draw(m);
        Dropdown d;
        if (open) {
            measure(m, &d);
            draw_dropdown(m, &d, sel);
        }
        xw_flush();
        int c = xw_next_key();
        if (c == K_RESIZE) continue;
        if (c == K_MOUSE) {
            if (xw_mouse.button != 1) continue;
            int t = menubar_hit(xw_mouse.x, xw_mouse.y);
            if (t >= 0) {
                if (open && t == m) return CMD_NONE;
                m = t; open = 1; sel = 0;
                continue;
            }
            if (open && xw_mouse.x >= d.x && xw_mouse.x < d.x + d.w &&
                xw_mouse.y >= d.y + DD_INSET && xw_mouse.y < d.y + d.h - DD_INSET)
                return choose(&menus[m].items[(xw_mouse.y - d.y - DD_INSET) / d.item_h]);
            return CMD_NONE;
        }
        if (c == 27 || c == K_F(9) || c == K_F(10) || c == K_CLOSE) return CMD_NONE;
        if (IS_ALT(c)) {
            int k = menubar_for_letter(c - K_ALT0);
            if (k >= 0) { m = k; open = 1; sel = 0; }
            continue;
        }
        if (!open) {
            int k = c > 0 && c < 128 ? menubar_for_letter(c) : -1;
            if (k >= 0) { m = k; open = 1; sel = 0; continue; }
            if (c == K_LEFT) m = (m + NMENUS - 1) % NMENUS;
            else if (c == K_RIGHT) m = (m + 1) % NMENUS;
            else if (c == K_DOWN || c == '\n') { open = 1; sel = 0; }
            continue;
        }
        int cnt = d.cnt;
        switch (c) {
        case K_LEFT:  m = (m + NMENUS - 1) % NMENUS; sel = 0; break;
        case K_RIGHT: m = (m + 1) % NMENUS; sel = 0; break;
        case K_UP:    sel = (sel + cnt - 1) % cnt; break;
        case K_DOWN:  sel = (sel + 1) % cnt; break;
        case '\n':    return choose(&menus[m].items[sel]);
        default:
            if (c > 0 && c < 128) {
                for (int i = 0; i < cnt; i++)
                    if (d.hotpos[i] >= 0 && tolower((unsigned char)d.shown[i][d.hotpos[i]]) == tolower(c))
                        return choose(&menus[m].items[i]);
            }
            break;
        }
    }
}
