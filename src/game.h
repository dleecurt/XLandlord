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

/* game.h - the rules of The Landlord's Game (1906, with the Single Tax).
 *
 * This module knows nothing about X: it holds the state of a game and plays
 * it by the rules. Whenever a player has a choice to make, it asks that
 * player's Decider, a table of functions: the human player's Decider opens
 * dialogs (human.c), a computer player's thinks (ai.c). Whatever happens is
 * reported through the Hooks, so the window can show it.
 *
 * THE RULES IMPLEMENTED (numbers are the 1906 rule numbers)
 *   1  The cards are dealt round and each player may buy those dealt to him.
 *   2  Dice move the checkers; between the Chances one may move backward.
 *   3  $100 wages on reaching or passing MOTHER EARTH.
 *   4  $10 tax on the Absolute Necessities (doubled with many houses, see
 *      "advanced" below).
 *   5,6 Trespassers go to JAIL: $50 fine or a double to come out, at most
 *      three turns.
 *   7  Land rent; unowned lots are offered to the player and bid for.
 *   8  SPECULATION, the broker's game.
 *   9  Franchises: $5, MUNICIPAL CINCH ($25), railroad MONOPOLY ($10 / $20 /
 *      $50) and the TRUST ($40, profits divided).
 *   10 Central Park is free.  11 Chance cards.  12 The Poor House.
 *   13 LUXURY: $75 for a luxury card (counts 100), or move backward.
 *   14 Houses: $100, $10 rent each on top of the land rent.
 *   17 A double is an "official pass": jump the nine spaces between the next
 *      two corners.
 *   19 The game ends when a player has received his wages five times (the
 *      number can be set).
 *   The "advanced and scientific" rules (optional): taxes double at 10 and
 *   25 houses; land rent doubles for each house on every lot of a section;
 *   three houses at most on a lot.
 *   The SINGLE TAX can be voted in at any time by two players; then land
 *   rent goes to the PUBLIC TREASURY, which buys the utilities for the
 *   public, builds free colleges and raises wages.
 *
 * Where the 1906 rules leave something to "the players themselves" (Rule
 * 18) - borrowing, selling to raise money - this program settles it the way
 * README.md describes. */
#ifndef GAME_H
#define GAME_H

#include "board.h"

#define MAXPLAYERS  4
#define NAME_LEN    32
#define LOG_LINES   200
#define LOG_LEN     160

/* Where money goes. A player is 0..MAXPLAYERS-1. */
#define TO_TREASURY  (-1)       /* the PUBLIC TREASURY */
#define TO_MISC      (-2)       /* the MISCELLANEOUS pile (the bank: never runs out) */

#define NOBODY       (-1)       /* owner of a card still in the pack */

enum { DIFF_EASY, DIFF_MEDIUM, DIFF_HARD };

typedef struct {
    char name[NAME_LEN];
    int cpu;                    /* 0 = a human: at this screen (hot-seat) or, later, over the network */
    int difficulty;             /* DIFF_* for a computer player */
    int cash;
    int pos;                    /* space 0..39 */
    int in_jail;
    int jail_turns;             /* turns already spent in jail */
    int in_poorhouse;
    int natural_opp;            /* Single Tax: resting on a Natural Opportunity */
    int wages;                  /* times wages received (Rule 19) */
    int luxuries;               /* luxury cards held (count 100 each) */
    int education;              /* Single Tax: Education cards */
    int professors;             /* Single Tax: Professor cards (count 100 each) */
} Player;

typedef struct Game Game;

/* The questions a player is asked. Each returns the player's answer. */
typedef struct {
    /* Rule 1: which of the cards just dealt to p does he buy? cards[0..n)
     * are space numbers; set want[i] to 1 for each one bought. */
    void (*deal_buy)(Game *g, int p, const int *cards, int n, int *want);
    /* Rule 7: p may buy space sp for `price` (the sale price, or the highest
     * bid if another player bid more). 1 = buy. */
    int (*buy)(Game *g, int p, int sp, int price, int high_bidder);
    /* Rule 7: another player landed on unowned sp; the most p would bid
     * (0 = no bid). `min` is the least acceptable bid. */
    int (*max_bid)(Game *g, int p, int sp, int min);
    /* Rule 6: pay the $50 fine (1) or try for a double (0)? */
    int (*jail_pay)(Game *g, int p);
    /* Rule 17: use the official pass? `with` / `without` are the spaces he
     * would end on. 1 = use it. */
    int (*use_pass)(Game *g, int p, int with, int without);
    /* Rule 2: forward (1) or backward (0)? */
    int (*direction)(Game *g, int p, int fwd, int back);
    /* Rule 8: move onto SPECULATION (1) or stay where he is (0)? */
    int (*speculate)(Game *g, int p);
    /* Rule 13: buy the luxury (1) or move backward (0)? */
    int (*luxury)(Game *g, int p);
    /* Single Tax Rule 4: build a house on the unimproved lot sp? */
    int (*st_build)(Game *g, int p, int sp);
    /* p must find `need` dollars more. He sells something (game_sell_*) and
     * returns 1, or returns 0 to give up (Poor House). */
    int (*raise)(Game *g, int p, int need);
    /* Vote on the Single Tax proposed by `proposer`. 1 = yes. */
    int (*vote_single_tax)(Game *g, int p, int proposer);
    /* Join `other` in a railroad TRUST? 1 = yes. */
    int (*join_trust)(Game *g, int p, int other);
    /* A computer player's own business before it throws: build, trade,
     * propose the Single Tax. NULL for the human (who uses the menus). */
    void (*pre_roll)(Game *g, int p);
} Decider;

/* What the game tells the window. Any of these may be NULL. */
typedef struct {
    void (*changed)(Game *g);               /* redraw; a message was logged */
    void (*dice)(Game *g, int d1, int d2);  /* the dice were thrown */
    void (*chance)(Game *g, int p, const char *text);   /* a Chance card was drawn */
    void (*pause)(Game *g, int p);          /* a good moment to let the human watch */
} Hooks;

struct Game {
    int nplayers;
    Player pl[MAXPLAYERS];
    const Decider *dec[MAXPLAYERS];
    Hooks hooks;
    void *ui;                   /* for the hooks */

    int cur;                    /* whose turn */
    int turn_no;                /* turns played */
    int owner[NSPACES];         /* card holder, NOBODY when in the pack */
    int houses[NSPACES];        /* houses on a lot */
    int house_owner[NSPACES];   /* who built them (Single Tax: not the land owner) */
    int public_owned[NSPACES];  /* Single Tax: utility taken over by the public */
    int colleges[NSPACES];      /* Single Tax: free college built on a trespass space */
    int treasury;               /* the PUBLIC TREASURY */
    int single_tax;             /* the Single Tax is in force */
    int st_progress;            /* Single Tax: steps of Rule 6 taken */
    int wage;                   /* current wages ($100; more under the Single Tax) */
    int trust_a, trust_b;       /* railroad TRUST partners, or -1 */
    int trust_refused;          /* the TRUST was offered and refused */
    int advanced;               /* the "advanced and scientific" rules */
    int end_wages;              /* the game ends at this many wages (Rule 19) */
    int over;                   /* the game has ended */
    int last_d1, last_d2;       /* the last throw */
    int chance_deck[32];        /* shuffled order of the red pack */
    int chance_next;
    unsigned long rng;

    char log[LOG_LINES][LOG_LEN];   /* the most recent messages, a ring */
    int log_count;                  /* total ever logged */
};

typedef struct {
    int nplayers;               /* 2..4 seats, in turn order */
    int cpu[MAXPLAYERS];        /* 1 = a computer player, 0 = a human */
    int difficulty[MAXPLAYERS]; /* per player (the human's is ignored) */
    char names[MAXPLAYERS][NAME_LEN];
    int advanced;
    int end_wages;
    unsigned long seed;
} GameSettings;

/* Set up a new game: money (Rule: $600 / $500 / $400), checkers on MOTHER
 * EARTH, the pack and the red pack shuffled. Deciders are set by the caller
 * (game_set_decider) before game_deal. */
void game_new(Game *g, const GameSettings *s);
void game_set_decider(Game *g, int p, const Decider *d);

/* Rule 1: deal the cards round and let each player buy his. */
void game_deal(Game *g);

/* Play the current player's whole turn and pass the dice on. */
void game_turn(Game *g);

/* ---- things a player may do in his turn, before throwing ---- */

/* Build a house on lot sp (Rule 14; Single Tax Rule 4). Returns NULL on
 * success, or why not. */
const char *game_build(Game *g, int p, int sp);
/* Can p build on sp now? */
int game_can_build(const Game *g, int p, int sp);

/* Sell card sp from `seller` to `buyer` for `price`. Returns NULL or why not. */
const char *game_trade(Game *g, int seller, int buyer, int sp, int price);

/* Propose the Single Tax: every other player votes; two yes votes (the
 * proposer's included) put it in force. Returns 1 if adopted. */
int game_propose_single_tax(Game *g, int p);

/* ---- raising money (Rule 12 and Rule 18) ---- */

/* Sell a card back to the pack for half its sale price; sell one house back
 * to the MISCELLANEOUS pile for $50. Return NULL or why not. */
const char *game_sell_to_pack(Game *g, int p, int sp);
const char *game_sell_house(Game *g, int p, int sp);
#define PACK_PRICE(sp)  (board[sp].price / 2)
#define HOUSE_PRICE     100
#define HOUSE_RESALE    50

/* ---- questions about the game ---- */

/* What p would pay landing on sp now (rent, fare, tax...), ignoring cards
 * still in the pack. */
int game_landing_cost(const Game *g, int p, int sp);
int game_land_rent(const Game *g, int sp);      /* rent of a lot, without houses */
int game_rail_fare(const Game *g, int sp);      /* fare on a railroad */
int game_tax(const Game *g);                    /* tax on an Absolute Necessity */
int game_cards_of(const Game *g, int p, int type);  /* cards held of a SpaceType (-1: all) */
int game_houses_of(const Game *g, int p);
int game_houses_total(const Game *g);
/* Net worth as counted at the end (Rule: every card, house and luxury counts
 * 100; under the Single Tax title deeds to lots count nothing). */
int game_worth(const Game *g, int p);
/* The leading player (highest worth). */
int game_leader(const Game *g);
/* The section (0..7) of a space between a corner and a railroad, or -1. */
int game_section(int sp);

/* Append a message to the log (printf-style) and tell the window. */
void game_log(Game *g, const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
/* The i-th most recent message (0 = newest), or NULL. */
const char *game_log_line(const Game *g, int i);

/* The game's dice and random numbers (seeded, so tests repeat). */
int game_random(Game *g, int n);    /* 0..n-1 */

#endif
