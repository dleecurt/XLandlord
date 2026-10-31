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

/* board.h - the board of The Landlord's Game, as data.
 *
 * The board is a circuit of 40 spaces: four corners and nine spaces on each
 * side between them (the 1906 rules let a player with an "official pass"
 * jump "the nine spaces between the next two corners"). Spaces are numbered
 * 0..39 in the direction of play, starting at MOTHER EARTH.
 *
 * WHERE THE DATA COMES FROM
 *   - The kinds of space, their colors, the sale prices of the lots, the
 *     railroad and franchise names, the taxes, fines and wages: the rules
 *     published by the Economic Game Co., New York, in 1906, under Lizzie J.
 *     Magie's patent (US 748,626, 1904). Both are in the public domain.
 *   - Fixed points stated by those rules: THE PIKE is the 6th space,
 *     BEGGARMAN'S COURT the 14th, SHELTER is right after JAIL, and LUXURY is
 *     six spaces past MADISON SQUARE, which is six past SLAMBANG TROLLEY.
 *   - Everything the surviving rule text does not say (the exact order of
 *     the other spaces, the land rent of each lot, the Chance cards) is
 *     reconstructed for this program; README.md lists what and why. */
#ifndef BOARD_H
#define BOARD_H

#define NSPACES     40
#define SIDE        10          /* spaces from one corner to the next */

/* Corners, by space number. */
#define SP_MOTHER_EARTH  0
#define SP_JAIL         10
#define SP_PARK         20
#define SP_POORHOUSE    30

/* Spaces between these two Chances may be moved in either direction (Rule 2). */
#define CHANCE_A        32
#define CHANCE_B        39

typedef enum {
    T_MOTHER,       /* the beginning point: wages when reached or passed */
    T_JAIL,         /* corner: where trespassers go */
    T_PARK,         /* Central Park: free */
    T_POOR,         /* the Poor House */
    T_LOT,          /* green: a lot, for sale or rent */
    T_RAILROAD,     /* pink: a railroad franchise */
    T_FRANCHISE,    /* yellow: a lighting or trolley franchise */
    T_TAX,          /* blue: an Absolute Necessity, taxed */
    T_TRESPASS,     /* land held out of use: go to jail */
    T_CHANCE,       /* draw a red card */
    T_LUXURY,       /* purple: buy a luxury or move backward */
    T_SPECULATION,  /* the stock broker's game */
    T_LEGACY        /* a legacy of $100 (from the 1904 patent) */
} SpaceType;

typedef struct {
    const char *name;       /* full name, for messages and dialogs */
    const char *label;      /* name broken into short lines for the board ('\n') */
    SpaceType type;
    int price;              /* sale price of the card (0: none) */
    int rent;               /* land rent of a lot (0 for other spaces) */
} Space;

extern const Space board[NSPACES];

/* Does this space have a card (title deed, charter or broker's license)? */
int board_has_card(int sp);

/* The order in which the public takes over the utilities under the Single
 * Tax (Rule 6): lighting, trolley, then the railroads round the board from
 * P.D.Q. R.R. Ends with -1. */
extern const int public_order[];

/* Chance cards (the red pack). */
typedef enum {
    CH_COLLECT,         /* collect `amount` from the MISCELLANEOUS pile */
    CH_PAY,             /* pay `amount` into the PUBLIC TREASURY */
    CH_GOTO,            /* move forward to space `amount` (wages if passing MOTHER EARTH) */
    CH_JAIL,            /* go straight to jail */
    CH_BACK,            /* move back `amount` spaces */
    CH_NEXT_RR,         /* move forward to the next railroad */
    CH_PER_HOUSE,       /* pay `amount` for each house you own */
    CH_PER_LOT,         /* collect `amount` for each lot you own */
    CH_FROM_EACH,       /* collect `amount` from every other player */
    CH_TO_EACH          /* pay `amount` to every other player */
} ChanceKind;

typedef struct {
    const char *text;
    ChanceKind kind;
    int amount;
} Chance;

extern const Chance chance_cards[];
extern const int nchance_cards;

#endif
