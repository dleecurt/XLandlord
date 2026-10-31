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

/* board.c - see board.h. */
#include "board.h"

/* Land rents follow the 1906 Land Rent Table, whose base rents run from $2 to
 * $22 in steps of $2: the cheapest lots near MOTHER EARTH rent for $2, the
 * dearest on the last side for $22. */
const Space board[NSPACES] = {
    /*  0 */ { "Mother Earth",              "MOTHER\nEARTH",          T_MOTHER,      0,  0 },
    /*  1 */ { "Lonely Lane",               "Lonely\nLane",           T_LOT,        25,  2 },
    /*  2 */ { "Food",                      "FOOD",                   T_TAX,         0,  0 },
    /*  3 */ { "Goat Alley",                "Goat\nAlley",            T_LOT,        50,  4 },
    /*  4 */ { "Chance",                    "CHANCE",                 T_CHANCE,      0,  0 },
    /*  5 */ { "P. D. Q. R. R.",            "P.D.Q.\nR.R.",           T_RAILROAD,   50,  0 },
    /*  6 */ { "The Pike",                  "The\nPike",              T_LOT,        25,  2 },
    /*  7 */ { "Legacy",                    "LEGACY",                 T_LEGACY,      0,  0 },
    /*  8 */ { "Market Place",              "Market\nPlace",          T_LOT,        50,  4 },
    /*  9 */ { "Cottage Terrace",           "Cottage\nTerrace",       T_LOT,        50,  6 },
    /* 10 */ { "Jail",                      "JAIL",                   T_JAIL,        0,  0 },
    /* 11 */ { "Shelter",                   "SHELTER",                T_TAX,         0,  0 },
    /* 12 */ { "Boomtown",                  "Boom-\ntown",            T_LOT,        50,  6 },
    /* 13 */ { "Lord Blueblood's Estate",   "Lord\nBlueblood's\nEstate", T_TRESPASS, 0,  0 },
    /* 14 */ { "Beggarman's Court",         "Beggar-\nman's\nCourt",  T_LOT,        50,  6 },
    /* 15 */ { "Gee Whiz R. R.",            "Gee Whiz\nR.R.",         T_RAILROAD,   50,  0 },
    /* 16 */ { "Soakum Lighting System",    "Soakum\nLighting",       T_FRANCHISE,  50,  0 },
    /* 17 */ { "Easy Street",               "Easy\nStreet",           T_LOT,        75,  8 },
    /* 18 */ { "Fuel",                      "FUEL",                   T_TAX,         0,  0 },
    /* 19 */ { "The Bowery",                "The\nBowery",            T_LOT,        75, 10 },
    /* 20 */ { "Central Park",              "CENTRAL\nPARK",          T_PARK,        0,  0 },
    /* 21 */ { "Speculation",               "SPECU-\nLATION",         T_SPECULATION,50,  0 },
    /* 22 */ { "Slambang Trolley",          "Slambang\nTrolley",      T_FRANCHISE,  50,  0 },
    /* 23 */ { "Fairhope Avenue",           "Fairhope\nAvenue",       T_LOT,        75, 10 },
    /* 24 */ { "Chance",                    "CHANCE",                 T_CHANCE,      0,  0 },
    /* 25 */ { "Shooting Star R. R.",       "Shooting\nStar R.R.",    T_RAILROAD,   50,  0 },
    /* 26 */ { "George Street",             "George\nStreet",         T_LOT,        75, 12 },
    /* 27 */ { "Clothing",                  "CLOTH-\nING",            T_TAX,         0,  0 },
    /* 28 */ { "Madison Square",            "Madison\nSquare",        T_LOT,       100, 14 },
    /* 29 */ { "D. F. Hogg's Game Preserves", "D.F. Hogg's\nGame\nPreserves", T_TRESPASS, 0, 0 },
    /* 30 */ { "Poor House",                "POOR\nHOUSE",            T_POOR,        0,  0 },
    /* 31 */ { "Grand Boulevard",           "Grand\nBoulevard",       T_LOT,       100, 16 },
    /* 32 */ { "Chance",                    "CHANCE",                 T_CHANCE,      0,  0 },
    /* 33 */ { "Wall Street",               "Wall\nStreet",           T_LOT,       100, 18 },
    /* 34 */ { "Luxury",                    "LUXURY",                 T_LUXURY,      0,  0 },
    /* 35 */ { "Royal Rusher R. R.",        "Royal\nRusher R.R.",     T_RAILROAD,   50,  0 },
    /* 36 */ { "Broadway",                  "Broad-\nway",            T_LOT,       100, 20 },
    /* 37 */ { "Luxury",                    "LUXURY",                 T_LUXURY,      0,  0 },
    /* 38 */ { "Fifth Avenue",              "Fifth\nAvenue",          T_LOT,       100, 22 },
    /* 39 */ { "Chance",                    "CHANCE",                 T_CHANCE,      0,  0 },
};

int board_has_card(int sp)
{
    if (sp < 0 || sp >= NSPACES) return 0;
    SpaceType t = board[sp].type;
    return t == T_LOT || t == T_RAILROAD || t == T_FRANCHISE || t == T_SPECULATION;
}

/* Rule 6 of the Single Tax: "SOAKUM LIGHTING SYSTEM ... then SLAMBANG
 * TROLLEY; then P.D.Q.R.R.; then GEE WIZZ R. R., and so on around the board
 * until all the railroads are free." */
const int public_order[] = { 16, 22, 5, 15, 25, 35, -1 };

/* The red pack. The 1906 Chance cards have not survived in print, so these
 * are new, written in the spirit of the game: the fortunes and misfortunes of
 * a wage earner in a land-monopoly economy. */
const Chance chance_cards[] = {
    { "Coal has gone up again. Pay $10 into the Public Treasury.",            CH_PAY,       10 },
    { "Found a purse on Broadway. Collect $25.",                               CH_COLLECT,   25 },
    { "A rich uncle remembers you. Collect a legacy of $100.",                 CH_COLLECT,  100 },
    { "Caught gathering firewood on Lord Blueblood's Estate. Go to Jail.",     CH_JAIL,       0 },
    { "Go back to Mother Earth and collect your wages.",                       CH_GOTO,       0 },
    { "Take a trip on the next railroad and pay the fare.",                    CH_NEXT_RR,    0 },
    { "The doctor's bill comes due. Pay $25.",                                 CH_PAY,       25 },
    { "You sell a painting at the fair. Collect $50.",                         CH_COLLECT,   50 },
    { "The street is paved and you are assessed. Pay $10 for each house.",     CH_PER_HOUSE, 10 },
    { "You forgot your umbrella. Go back three spaces.",                       CH_BACK,       3 },
    { "A tip on the stock market goes wrong. Pay $50.",                        CH_PAY,       50 },
    { "Elected to the Board of Aldermen: every player pays you $10.",          CH_FROM_EACH, 10 },
    { "A holiday! Take a stroll to Central Park.",                             CH_GOTO,      20 },
    { "The new trolley line raises land values: collect $5 for each lot.",     CH_PER_LOT,    5 },
    { "You give a dinner for your friends. Pay every player $10.",             CH_TO_EACH,   10 },
    { "Your work is praised and your wages raised. Collect $30.",              CH_COLLECT,   30 },
};
const int nchance_cards = (int)(sizeof chance_cards / sizeof chance_cards[0]);
