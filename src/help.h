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

/* help.h - the texts of the Help menu (NULL-terminated arrays of lines). */
#ifndef HELP_H
#define HELP_H

/* The installed documents (make passes the configured docdir); the rule
 * book PDF is shown to the player by name in the Help texts. */
#ifndef DOCDIR
#define DOCDIR "/usr/local/share/doc/xlandlord"
#endif
#define RULEBOOK_PDF DOCDIR "/xlandlord-rules.pdf"

extern const char *const help_rules[];
extern const char *const help_history[];
extern const char *const help_keys[];
extern const char *const help_network[];
extern const char *const help_license[];

#endif
