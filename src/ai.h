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

/* ai.h - the computer players, at three levels of difficulty.
 *
 *   EASY    plays on impulse: buys what it can afford about half the time,
 *           never builds much, never bids, picks moves at random, and keeps
 *           next to no money in hand.
 *   MEDIUM  plays sensibly by rules of thumb: buys and builds while it keeps
 *           a $100 cushion, bids a little over the price for good cards,
 *           pays its way out of Jail early in the game, and avoids an
 *           obviously expensive square when it has a choice.
 *   HARD    counts: every card and house is worth 100 at the end, so it
 *           values a card by that plus the rent it can expect to collect in
 *           the circuits left, works out what it is likely to have to pay on
 *           the way round and keeps that much in hand, goes after railroad
 *           monopolies and the Municipal Cinch (and blocks yours), bids up
 *           to what a card is worth to it, stays in Jail late in the game
 *           when the board is dangerous, chooses the cheapest square for a
 *           pass or a backward move, trades with the other computer
 *           players, and votes on the Single Tax by what it does to its
 *           standing. */
#ifndef AI_H
#define AI_H

#include "game.h"

/* The Decider for every computer player. */
extern const Decider ai_decider;

/* What a computer player thinks card sp is worth to it, in dollars. */
int ai_card_value(Game *g, int p, int sp);

/* The most computer player p would pay for card sp (0 = not interested). */
int ai_offer_for(Game *g, int p, int sp);

/* Would computer player p sell its card sp for `price`? */
int ai_accepts_sale(Game *g, int p, int sp, int price);

/* The money a computer player likes to keep in hand. */
int ai_buffer(Game *g, int p);

#endif
