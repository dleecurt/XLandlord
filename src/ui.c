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

/* ui.c - see ui.h. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "keys.h"
#include "util.h"
#include "view.h"
#include "xwin.h"

/* ---- building blocks ---- */

int ui_hit(UiRect r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* The height of a dialog's title with its etched line. */
static int title_height(void) { return xw_line_height(XF_UI) + 12; }

/* See ui.h. */
int ui_title_height(void) { return title_height(); }

/* Return or keypad Enter. */
static int is_enter(int k) { return k == '\n' || k == '\r'; }

/* See ui.h: draw the game, then an empty dialog panel with its title;
 * returns the area inside. */
UiRect ui_frame(const char *title, int w, int h)
{
    view_draw();
    int W = xw_width(), H = xw_height();
    w = MIN(w, W - 16);
    h = MIN(h, H - 16);
    int x = (W - w) / 2, y = (H - h) / 2;
    xw_fill(x, y, w, h, XC_SURFACE);
    xw_box(x, y, w, h, XC_TROUGH);              /* Viola's popup outline */
    xw_raised(x + 1, y + 1, w - 2, h - 2);
    int top = y + UI_MARGIN;
    if (title) {
        xw_clip(x + 3, y + 3, w - 6, h - 6);
        xw_text(XF_UI, x + UI_MARGIN, top + xw_ascent(XF_UI), title, (int)strlen(title), XC_INK);
        xw_clip_off();
        int ly = top + xw_line_height(XF_UI) + 5;
        xw_hline(x + UI_MARGIN, ly, w - 2 * UI_MARGIN, XC_DARK);   /* an etched line */
        xw_hline(x + UI_MARGIN, ly + 1, w - 2 * UI_MARGIN, XC_LIGHT);
        top += title_height();
    }
    return (UiRect){ x + UI_MARGIN, top, w - 2 * UI_MARGIN, y + h - UI_MARGIN - top };
}

/* Remove the & from a button label into `out`; returns the hotkey's
 * position or -1. */
static int split_amp(const char *label, char *out, size_t size)
{
    int pos = -1;
    size_t o = 0;
    for (const char *p = label; *p && o + 1 < size; p++) {
        if (*p == '&' && p[1]) { pos = (int)o; continue; }
        out[o++] = *p;
    }
    out[o] = '\0';
    return pos;
}

/* The lowercase hotkey letter of a label, or 0. */
static int hotkey_of(const char *label)
{
    char shown[64];
    int pos = split_amp(label, shown, sizeof shown);
    return pos >= 0 ? tolower((unsigned char)shown[pos]) : 0;
}

/* See ui.h: a raised TextButton, its hotkey underlined, the focus box when
 * focused. */
void ui_button(UiRect r, const char *label, int focused)
{
    char shown[64];
    int hot = split_amp(label, shown, sizeof shown);
    int n = (int)strlen(shown);
    xw_fill(r.x, r.y, r.w, r.h, XC_SURFACE);
    xw_raised(r.x, r.y, r.w, r.h);
    int tw = xw_text_width(XF_UI, shown, n);
    int tx = r.x + (r.w - tw) / 2;
    int by = r.y + (r.h - xw_line_height(XF_UI)) / 2 + xw_ascent(XF_UI);
    xw_text(XF_UI, tx, by, shown, n, XC_INK);
    if (hot >= 0)
        xw_hline(tx + xw_text_width(XF_UI, shown, hot), by + 2, xw_text_width(XF_UI, shown + hot, 1), XC_INK);
    if (focused) xw_box(r.x + 4, r.y + 4, r.w - 8, r.h - 8, XC_INK);
}

/* A choice button that is "pressed in" when selected (a Viola toggle). */
static void toggle_button(UiRect r, const char *label, int on, int focused)
{
    int n = (int)strlen(label);
    xw_fill(r.x, r.y, r.w, r.h, on ? XC_MARK : XC_SURFACE);
    if (on) xw_sunken(r.x, r.y, r.w, r.h);
    else xw_raised(r.x, r.y, r.w, r.h);
    int tw = xw_text_width(XF_UI, label, n);
    int by = r.y + (r.h - xw_line_height(XF_UI)) / 2 + xw_ascent(XF_UI);
    xw_text(XF_UI, r.x + (r.w - tw) / 2 + on, by + on, label, n, XC_INK);
    if (focused) xw_box(r.x + 4, r.y + 4, r.w - 8, r.h - 8, XC_INK);
}

/* See ui.h: a sunken edit field with a text cursor. */
void ui_field(UiRect r, const char *text, int cursor)
{
    xw_fill(r.x, r.y, r.w, r.h, XC_DARK);       /* Viola's edit-bg */
    xw_sunken(r.x, r.y, r.w, r.h);
    int n = (int)strlen(text);
    int by = r.y + (r.h - xw_line_height(XF_UI)) / 2 + xw_ascent(XF_UI);
    xw_clip(r.x + 2, r.y + 2, r.w - 4, r.h - 4);
    int used = xw_text(XF_UI, r.x + 6, by, text, n, XC_INK);
    if (cursor) xw_fill(r.x + 6 + used + 1, by - xw_ascent(XF_UI), 2, xw_line_height(XF_UI), XC_INK);
    xw_clip_off();
}

/* See ui.h: break text into lines at spaces (and at newlines). */
int ui_wrap(int font, const char *text, int width, char *buf, size_t size, const char **lines, int max)
{
    copy_str(buf, size, text);
    int n = 0;
    char *p = buf;
    while (*p && n < max) {
        char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        /* Take as many words as fit. */
        int cut = len;
        if (xw_text_width(font, p, len) > width) {
            int last_space = -1;
            for (int i = 0; i < len; i++) {
                if (p[i] == ' ') {
                    if (xw_text_width(font, p, i) > width) break;
                    last_space = i;
                }
            }
            cut = last_space > 0 ? last_space : len;
        }
        lines[n++] = p;
        if (cut < len || nl) {
            char *end = p + cut;
            int was_nl = *end == '\n';
            *end = '\0';
            p = end + 1;
            if (!was_nl) while (*p == ' ') p++;
        } else {
            p += len;
        }
    }
    return n;
}

/* ---- message dialogs ---- */

int ui_ask(const char *title, const char *text, const char *const *buttons, int n, int def, int esc)
{
    int focus = CLAMP(def, 0, n - 1);
    char buf[2048];
    const char *lines[32];
    for (;;) {
        int bw = UI_BUTTON_W;
        for (int i = 0; i < n; i++) {
            char shown[64];
            split_amp(buttons[i], shown, sizeof shown);
            bw = MAX(bw, xw_text_width(XF_UI, shown, (int)strlen(shown)) + 28);
        }
        int row_w = n * bw + (n - 1) * 10;
        int w = MIN(MAX(460, row_w + 2 * UI_MARGIN), xw_width() - 40);
        int nl = ui_wrap(XF_UI, text, w - 2 * UI_MARGIN, buf, sizeof buf, lines, 32);
        int lh = xw_line_height(XF_UI);
        int h = 2 * UI_MARGIN + (title ? title_height() : 0) + nl * lh + 18 + UI_BUTTON_H;
        UiRect c = ui_frame(title, w, h);
        for (int i = 0; i < nl; i++)
            xw_text(XF_UI, c.x, c.y + i * lh + xw_ascent(XF_UI), lines[i], (int)strlen(lines[i]), XC_INK);
        UiRect b[4];
        int bx = c.x + c.w - row_w;
        for (int i = 0; i < n && i < 4; i++) {
            b[i] = (UiRect){ bx + i * (bw + 10), c.y + c.h - UI_BUTTON_H, bw, UI_BUTTON_H };
            ui_button(b[i], buttons[i], i == focus);
        }
        xw_flush();

        int k = xw_next_key();
        if (k == K_MOUSE) {
            if (xw_mouse.button != 1) continue;
            for (int i = 0; i < n && i < 4; i++) if (ui_hit(b[i], xw_mouse.x, xw_mouse.y)) return i;
            continue;
        }
        if (is_enter(k) || k == ' ') return focus;
        if (k == 27 || k == K_CLOSE) { if (esc >= 0) return esc; continue; }
        if (k == K_LEFT || k == K_BTAB) focus = (focus + n - 1) % n;
        else if (k == K_RIGHT || k == '\t') focus = (focus + 1) % n;
        else if (k > 0 && k < 128) {
            for (int i = 0; i < n; i++) if (hotkey_of(buttons[i]) == tolower(k)) return i;
        }
    }
}

/* See ui.h. */
int ui_yes_no(const char *title, const char *text, int def_yes)
{
    static const char *const yn[] = { "&Yes", "&No" };
    return ui_ask(title, text, yn, 2, def_yes ? 0 : 1, 1) == 0;
}

/* See ui.h. */
void ui_message(const char *title, const char *text)
{
    static const char *const ok[] = { "OK" };
    ui_ask(title, text, ok, 1, 0, 0);
}

/* See ui.h: a thin Viola scroll bar (trough and thumb). */
void ui_scrollbar(int x, int y, int h, int top, int rows, int n)
{
    if (n <= rows) return;
    xw_fill(x, y, 12, h, XC_TROUGH);
    int len = MAX(10, h * rows / n);
    int pos = (h - len) * top / MAX(1, n - rows);
    xw_fill(x + 1, y + pos, 10, len, XC_SURFACE);
    xw_fill(x + 1, y + pos, 10, 2, XC_LIGHT);
    xw_fill(x + 1, y + pos, 2, len, XC_LIGHT);
}

/* See ui.h: arrows, Page Up/Down and the wheel scroll a long text. */
void ui_info_box(const char *title, const char *const *lines)
{
    int n = 0;
    while (lines[n]) n++;
    int top = 0;
    for (;;) {
        int widest = xw_text_width(XF_UI, title, (int)strlen(title));
        for (int i = 0; i < n; i++) widest = MAX(widest, xw_text_width(XF_FIXED, lines[i], (int)strlen(lines[i])));
        int lh = xw_line_height(XF_FIXED);
        int chrome = 2 * UI_MARGIN + title_height() + 14 + UI_BUTTON_H;
        int rows = MAX(1, MIN(n, (xw_height() - 40 - chrome) / lh));
        int scroll = n > rows;
        int w = MIN(widest + 2 * UI_MARGIN + (scroll ? 20 : 0) + 4, xw_width() - 40);
        top = CLAMP(top, 0, MAX(0, n - rows));
        UiRect c = ui_frame(title, MAX(w, 320), chrome + rows * lh);
        xw_clip(c.x, c.y, c.w - (scroll ? 18 : 0), rows * lh);
        for (int i = 0; i < rows && top + i < n; i++)
            xw_text(XF_FIXED, c.x, c.y + i * lh + xw_ascent(XF_FIXED), lines[top + i], (int)strlen(lines[top + i]), XC_INK);
        xw_clip_off();
        if (scroll) ui_scrollbar(c.x + c.w - 12, c.y, rows * lh, top, rows, n);
        UiRect ok = { c.x + c.w - UI_BUTTON_W, c.y + c.h - UI_BUTTON_H, UI_BUTTON_W, UI_BUTTON_H };
        ui_button(ok, "OK", 1);
        xw_flush();

        int k = xw_next_key();
        if (k == K_RESIZE) continue;
        if (k == K_MOUSE) {
            if (xw_mouse.button == 4) { top -= 3; continue; }
            if (xw_mouse.button == 5) { top += 3; continue; }
            if (scroll && !ui_hit(ok, xw_mouse.x, xw_mouse.y)) continue;
            return;
        }
        if (scroll) {
            if (k == K_UP)   { top--; continue; }
            if (k == K_DOWN) { top++; continue; }
            if (k == K_PGUP) { top -= rows - 1; continue; }
            if (k == K_PGDN || k == ' ') { top += rows - 1; continue; }
            if (k == K_HOME) { top = 0; continue; }
            if (k == K_END)  { top = n; continue; }
        }
        return;
    }
}

/* Two buttons at the bottom right of content area c. */
static void two_buttons(UiRect c, UiRect b[2])
{
    int y = c.y + c.h - UI_BUTTON_H;
    b[1] = (UiRect){ c.x + c.w - UI_BUTTON_W, y, UI_BUTTON_W, UI_BUTTON_H };
    b[0] = (UiRect){ b[1].x - 10 - UI_BUTTON_W, y, UI_BUTTON_W, UI_BUTTON_H };
}

/* ---- lists ---- */

/* The list dialogs share this: draws title, wrapped note, a sunken list
 * (with check boxes if `checked`), a footer line and OK / Cancel. */
typedef struct {
    UiRect list, ok, cancel;
    int rows, ih;
} ListGeom;

static void draw_list(const char *title, const char *note, const char *const *items, int n,
                      int sel, int *top, const int *checked, const char *footer, ListGeom *g)
{
    char buf[1024];
    const char *lines[12];
    int lh = xw_line_height(XF_UI), ih = lh + 8;
    int widest = xw_text_width(XF_UI, title, (int)strlen(title));
    for (int i = 0; i < n; i++) widest = MAX(widest, xw_text_width(XF_UI, items[i], (int)strlen(items[i])) + 40);
    int w = MIN(MAX(widest + 2 * UI_MARGIN + 24, 440), xw_width() - 40);
    int nn = note ? ui_wrap(XF_UI, note, w - 2 * UI_MARGIN, buf, sizeof buf, lines, 12) : 0;
    int noteh = nn ? nn * lh + 10 : 0;
    int footh = footer ? lh + 8 : 0;
    int chrome = 2 * UI_MARGIN + title_height() + noteh + footh + 14 + UI_BUTTON_H + 4;
    int rows = MAX(1, MIN(n, (xw_height() - 40 - chrome) / ih));
    int scroll = n > rows;
    if (sel < *top) *top = sel;
    if (sel >= *top + rows) *top = sel - rows + 1;
    UiRect c = ui_frame(title, w, chrome + rows * ih);
    for (int i = 0; i < nn; i++)
        xw_text(XF_UI, c.x, c.y + i * lh + xw_ascent(XF_UI), lines[i], (int)strlen(lines[i]), XC_INK);
    UiRect list = { c.x, c.y + noteh, c.w - (scroll ? 18 : 0), rows * ih + 4 };
    xw_fill(list.x, list.y, list.w, list.h, XC_SURFACE);
    xw_sunken(list.x, list.y, list.w, list.h);
    xw_clip(list.x + 2, list.y + 2, list.w - 4, list.h - 4);
    for (int i = 0; i < rows && *top + i < n; i++) {
        int k = *top + i, iy = list.y + 2 + i * ih;
        int tx = list.x + 10;
        if (k == sel) xw_box(list.x + 3, iy, list.w - 6, ih, XC_INK);
        if (checked) {          /* a Viola check box: a small sunken square, ink-filled when on */
            int bs = 12, bx = list.x + 10, byy = iy + (ih - bs) / 2;
            xw_fill(bx, byy, bs, bs, XC_DARK);
            xw_sunken(bx, byy, bs, bs);
            if (checked[k]) xw_fill(bx + 3, byy + 3, bs - 6, bs - 6, XC_INK);
            tx += bs + 10;
        }
        xw_text(XF_UI, tx, iy + (ih - lh) / 2 + xw_ascent(XF_UI), items[k], (int)strlen(items[k]), XC_INK);
    }
    xw_clip_off();
    if (scroll) ui_scrollbar(c.x + c.w - 12, list.y, list.h, *top, rows, n);
    if (footer)
        xw_text(XF_UI, c.x, list.y + list.h + 6 + xw_ascent(XF_UI), footer, (int)strlen(footer), XC_INK);
    UiRect b[2];
    two_buttons(c, b);
    ui_button(b[0], "OK", 1);
    ui_button(b[1], "Cancel", 0);
    g->list = list; g->ok = b[0]; g->cancel = b[1]; g->rows = rows; g->ih = ih;
}

/* See ui.h: arrows move, Enter or a double click chooses. */
int ui_choose(const char *title, const char *note, const char *const *items, int n, int initial)
{
    if (n <= 0) return -1;
    int sel = CLAMP(initial, 0, n - 1), top = 0;
    for (;;) {
        ListGeom g;
        draw_list(title, note, items, n, sel, &top, NULL, NULL, &g);
        xw_flush();
        int k = xw_next_key();
        if (k == K_MOUSE) {
            int mx = xw_mouse.x, my = xw_mouse.y;
            if (xw_mouse.button == 4) { if (sel > 0) sel--; continue; }
            if (xw_mouse.button == 5) { if (sel < n - 1) sel++; continue; }
            if (xw_mouse.button != 1) continue;
            if (ui_hit(g.ok, mx, my)) return sel;
            if (ui_hit(g.cancel, mx, my)) return -1;
            if (ui_hit(g.list, mx, my)) {
                int i = top + (my - g.list.y - 2) / g.ih;
                if (i >= 0 && i < n) { sel = i; if (xw_mouse.clicks == 2) return sel; }
            }
            continue;
        }
        if (k == K_UP && sel > 0) sel--;
        else if (k == K_DOWN && sel < n - 1) sel++;
        else if (k == K_PGUP) sel = MAX(0, sel - g.rows);
        else if (k == K_PGDN) sel = MIN(n - 1, sel + g.rows);
        else if (k == K_HOME) sel = 0;
        else if (k == K_END) sel = n - 1;
        else if (is_enter(k)) return sel;
        else if (k == 27 || k == K_CLOSE) return -1;
    }
}

int ui_checklist(const char *title, const char *note, const char *const *items, int n, int *checked,
                 void (*footer)(const int *checked, char *out, size_t size))
{
    int sel = 0, top = 0;
    if (n <= 0) return 1;
    for (;;) {
        char foot[160] = "";
        if (footer) footer(checked, foot, sizeof foot);
        ListGeom g;
        draw_list(title, note, items, n, sel, &top, checked, footer ? foot : NULL, &g);
        xw_flush();
        int k = xw_next_key();
        if (k == K_MOUSE) {
            int mx = xw_mouse.x, my = xw_mouse.y;
            if (xw_mouse.button == 4) { if (sel > 0) sel--; continue; }
            if (xw_mouse.button == 5) { if (sel < n - 1) sel++; continue; }
            if (xw_mouse.button != 1) continue;
            if (ui_hit(g.ok, mx, my)) return 1;
            if (ui_hit(g.cancel, mx, my)) return 0;
            if (ui_hit(g.list, mx, my)) {
                int i = top + (my - g.list.y - 2) / g.ih;
                if (i >= 0 && i < n) { sel = i; checked[i] = !checked[i]; }
            }
            continue;
        }
        if (k == K_UP && sel > 0) sel--;
        else if (k == K_DOWN && sel < n - 1) sel++;
        else if (k == ' ') checked[sel] = !checked[sel];
        else if (k == 'a' || k == 'A') { for (int i = 0; i < n; i++) checked[i] = 1; }
        else if (k == 'n' || k == 'N') { for (int i = 0; i < n; i++) checked[i] = 0; }
        else if (is_enter(k)) return 1;
        else if (k == 27 || k == K_CLOSE) return 0;
    }
}

/* ---- text entry ---- */

static int entry(const char *title, const char *note, char *buf, size_t size, int digits_only)
{
    size_t len = strlen(buf);
    int fresh = 1;              /* the first character typed replaces the suggestion */
    char nbuf[1024];
    const char *lines[12];
    for (;;) {
        int w = MIN(520, xw_width() - 40);
        int lh = xw_line_height(XF_UI);
        int nn = note ? ui_wrap(XF_UI, note, w - 2 * UI_MARGIN, nbuf, sizeof nbuf, lines, 12) : 0;
        int h = 2 * UI_MARGIN + title_height() + nn * lh + (nn ? 10 : 0) + UI_FIELD_H + 16 + UI_BUTTON_H;
        UiRect c = ui_frame(title, w, h);
        for (int i = 0; i < nn; i++)
            xw_text(XF_UI, c.x, c.y + i * lh + xw_ascent(XF_UI), lines[i], (int)strlen(lines[i]), XC_INK);
        UiRect field = { c.x, c.y + nn * lh + (nn ? 10 : 0), c.w, UI_FIELD_H };
        UiRect b[2];
        two_buttons(c, b);
        ui_field(field, buf, 1);
        ui_button(b[0], "OK", 1);
        ui_button(b[1], "Cancel", 0);
        xw_flush();
        int k = xw_next_key();
        if (k >= 32 && k < 127 && fresh) { len = 0; buf[0] = '\0'; }
        if (k != K_RESIZE && k != K_MOUSE) fresh = 0;
        if (k == K_MOUSE) {
            if (xw_mouse.button != 1) continue;
            if (ui_hit(b[0], xw_mouse.x, xw_mouse.y)) return 1;
            if (ui_hit(b[1], xw_mouse.x, xw_mouse.y)) return 0;
        }
        else if (is_enter(k)) return 1;
        else if (k == 27 || k == K_CLOSE) return 0;
        else if (k == K_BACKSPACE) { if (len) buf[--len] = '\0'; }
        else if (k == 21) { len = 0; buf[0] = '\0'; }
        else if (k >= 32 && k < 127 && len + 1 < size && (!digits_only || isdigit(k))) {
            buf[len++] = (char)k;
            buf[len] = '\0';
        }
    }
}

/* See ui.h: digits only. */
int ui_number(const char *title, const char *note, int *value)
{
    char buf[12];
    snprintf(buf, sizeof buf, "%d", MAX(0, *value));
    if (!entry(title, note, buf, sizeof buf, 1)) return 0;
    *value = atoi(buf);
    return 1;
}

/* See ui.h. */
int ui_prompt(const char *title, const char *note, char *buf, size_t size)
{
    return entry(title, note, buf, size, 0);
}

/* ---- the form ---- */

int ui_form(const char *title, const char *note, const UiRow *rows, int nrows, int *sel, const char *ok)
{
    int row = 0, focus_ok = 0;
    char nbuf[1024];
    const char *lines[12];
    for (;;) {
        int lh = xw_line_height(XF_UI), rh = UI_BUTTON_H + 10;
        int labw = 0, cw = 70, maxc = 0;
        for (int r = 0; r < nrows; r++) {
            labw = MAX(labw, xw_text_width(XF_UI, rows[r].label, (int)strlen(rows[r].label)));
            maxc = MAX(maxc, rows[r].nchoices);
            for (int i = 0; i < rows[r].nchoices; i++)
                cw = MAX(cw, xw_text_width(XF_UI, rows[r].choices[i], (int)strlen(rows[r].choices[i])) + 24);
        }
        int w = MIN(2 * UI_MARGIN + labw + 16 + maxc * (cw + 6), xw_width() - 40);
        w = MAX(w, 420);
        int nn = note ? ui_wrap(XF_UI, note, w - 2 * UI_MARGIN, nbuf, sizeof nbuf, lines, 12) : 0;
        int noteh = nn ? nn * lh + 12 : 0;
        int h = 2 * UI_MARGIN + title_height() + noteh + nrows * rh + 14 + UI_BUTTON_H;
        UiRect c = ui_frame(title, w, h);
        for (int i = 0; i < nn; i++)
            xw_text(XF_UI, c.x, c.y + i * lh + xw_ascent(XF_UI), lines[i], (int)strlen(lines[i]), XC_INK);
        UiRect cells[8][6];
        for (int r = 0; r < nrows && r < 8; r++) {
            int y = c.y + noteh + r * rh;
            xw_text(XF_UI, c.x, y + (UI_BUTTON_H - lh) / 2 + xw_ascent(XF_UI), rows[r].label,
                    (int)strlen(rows[r].label), XC_INK);
            for (int i = 0; i < rows[r].nchoices && i < 6; i++) {
                cells[r][i] = (UiRect){ c.x + labw + 16 + i * (cw + 6), y, cw, UI_BUTTON_H };
                toggle_button(cells[r][i], rows[r].choices[i], sel[r] == i, !focus_ok && r == row && sel[r] == i);
            }
        }
        UiRect b[2];
        two_buttons(c, b);
        ui_button(b[0], ok, focus_ok);
        ui_button(b[1], "Cancel", 0);
        xw_flush();

        int k = xw_next_key();
        if (k == K_MOUSE) {
            if (xw_mouse.button != 1) continue;
            if (ui_hit(b[0], xw_mouse.x, xw_mouse.y)) return 1;
            if (ui_hit(b[1], xw_mouse.x, xw_mouse.y)) return 0;
            for (int r = 0; r < nrows && r < 8; r++)
                for (int i = 0; i < rows[r].nchoices && i < 6; i++)
                    if (ui_hit(cells[r][i], xw_mouse.x, xw_mouse.y)) { sel[r] = i; row = r; focus_ok = 0; }
            continue;
        }
        if (is_enter(k)) return 1;
        if (k == 27 || k == K_CLOSE) return 0;
        if (k == '\t' || k == K_DOWN) { if (focus_ok) { focus_ok = 0; row = 0; } else if (++row >= nrows) focus_ok = 1; }
        else if (k == K_BTAB || k == K_UP) { if (focus_ok) { focus_ok = 0; row = nrows - 1; } else if (row > 0) row--; }
        else if (!focus_ok && k == K_LEFT && sel[row] > 0) sel[row]--;
        else if (!focus_ok && k == K_RIGHT && sel[row] < rows[row].nchoices - 1) sel[row]++;
    }
}
