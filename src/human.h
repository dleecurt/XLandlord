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

/* Hot-seat trades between two humans at this screen.
 * human_accepts_sale: `buyer` offers `price` for `seller`'s card sp; the
 *   seller is asked. human_accepts_purchase: `seller` asks `price` of
 *   `buyer`; the buyer is asked. Each returns 1 for yes. */
int human_accepts_sale(Game *g, int seller, int buyer, int sp, int price);
int human_accepts_purchase(Game *g, int buyer, int seller, int sp, int price);

/* Raise money interactively outside a payment (Game > Sell to the Pack):
 * show the list of things p can sell; returns 1 if something was sold. */
int human_sell_menu(Game *g, int p, const char *title, const char *note, int need);

/* The same list, only choosing: kind 1 a house, 2 to computer player
 * `buyer` (at about `price`), 3 back to the pack. Returns 1 if chosen.
 * (A joiner chooses on his screen; the host carries the sale out.) */
int human_choose_sale(Game *g, int p, const char *title, const char *note, int need,
                      int *kind, int *sp, int *buyer, int *price);

/* Rule 12's question, choosing only: what to sell to find `need` dollars,
 * or 0 to give up (Poor House). */
int human_raise_choice(Game *g, int p, int need, int *kind, int *sp, int *buyer);

#endif
