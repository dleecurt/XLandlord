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

/* ui.h - Viola-style dialogs.
 *
 * Every dialog is a small loop: draw the game underneath (view_draw) and the
 * dialog on top, show it, wait for a key or a click, act, repeat. Drawing
 * everything on every pass means a resize is never a special case.
 *
 * The look, after the Viola design system (and Ted Viola): a gray94 panel
 * with a trough-colored outline and a raised bevel; the title in Helvetica
 * over an etched line; TextButtons raised, the default one marked by a 1px
 * black focus rectangle inside it; edit fields sunken on bevel-dark; list
 * selections marked by a 1px black box, never a fill. */
#ifndef UI_H
#define UI_H

#include <stddef.h>

typedef struct { int x, y, w, h; } UiRect;

#define UI_MARGIN   14
#define UI_BUTTON_H 30
#define UI_BUTTON_W 96
#define UI_FIELD_H  30

int    ui_hit(UiRect r, int x, int y);
UiRect ui_frame(const char *title, int w, int h);
void   ui_button(UiRect r, const char *label, int focused);
void   ui_field(UiRect r, const char *text, int cursor);

/* Break `text` into lines no wider than `width` pixels in `font`. Lines
 * point into `buf` (size bytes). Returns the number of lines (<= max). */
int ui_wrap(int font, const char *text, int width, char *buf, size_t size, const char **lines, int max);

/* A message with one to four buttons ("&Yes" marks Y as its key). Enter
 * presses the focused button (initially `def`), Esc gives -1 unless
 * `esc_ok` is a button index to return instead. Returns the button chosen. */
int ui_ask(const char *title, const char *text, const char *const *buttons, int n, int def, int esc);

/* Yes / No. Returns 1 for yes. */
int ui_yes_no(const char *title, const char *text, int def_yes);

/* A message and OK. */
void ui_message(const char *title, const char *text);

/* A long text (lines, NULL-terminated) in the fixed font, scrollable, OK. */
void ui_info_box(const char *title, const char *const *lines);

/* A thin Viola scroll bar, 12 pixels wide and h tall at (x, y), for a list
 * of n rows showing `rows` of them from `top`; nothing if all fit. */
void ui_scrollbar(int x, int y, int h, int top, int rows, int n);

/* The height of a dialog's title (with its etched line), for laying out
 * dialogs made with ui_frame. */
int ui_title_height(void);

/* Choose one of n items from a list. Returns the index or -1. */
int ui_choose(const char *title, const char *note, const char *const *items, int n, int initial);

/* Tick any of n items. `footer` (may be NULL) is called to describe the
 * current choice (e.g. the total cost) under the list. Returns 1 for OK. */
int ui_checklist(const char *title, const char *note, const char *const *items, int n, int *checked,
                 void (*footer)(const int *checked, char *out, size_t size));

/* Ask for a whole number of dollars. Returns 1 and sets *value on OK. */
int ui_number(const char *title, const char *note, int *value);

/* Ask for a line of text. Returns 1 on OK. */
int ui_prompt(const char *title, const char *note, char *buf, size_t size);

/* A form of rows, each a label and a set of mutually exclusive choices
 * shown as Viola buttons (the chosen one sunken). sel[i] is row i's choice.
 * Returns 1 if confirmed with the OK button (labelled `ok`). */
typedef struct {
    const char *label;
    const char *const *choices;
    int nchoices;
} UiRow;
int ui_form(const char *title, const char *note, const UiRow *rows, int nrows, int *sel, const char *ok);

#endif
