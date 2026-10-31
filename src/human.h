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

/* human.h - the Decider for the player at the keyboard: every question the
 * rules ask him becomes a Viola dialog. */
#ifndef HUMAN_H
#define HUMAN_H

#include "game.h"

extern const Decider human_decider;

/* Raise money interactively outside a payment (Game > Sell to the Pack):
 * show the list of things p can sell; returns 1 if something was sold. */
int human_sell_menu(Game *g, int p, const char *title, const char *note, int need);

#endif
