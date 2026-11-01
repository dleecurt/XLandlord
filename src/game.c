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

/* game.c - see game.h.
 *
 * A turn, in outline (game_turn):
 *   1. a player leaving the Poor House or a Natural Opportunity is let out;
 *   2. a computer player does its business (pre_roll): builds, trades,
 *      perhaps proposes the Single Tax;
 *   3. a player in JAIL pays the fine or tries for a double;
 *   4. the dice are thrown and the checker moves (move): direction between
 *      the Chances, the official pass on a double, the choice to stay put
 *      rather than speculate;
 *   5. the space is settled (land): rent, fares, taxes, cards, luxuries...
 *   6. the game ends when someone has received his wages end_wages times.
 *
 * Money: the MISCELLANEOUS pile is the bank, it never runs out. The PUBLIC
 * TREASURY collects sale prices, taxes and fines; under the Single Tax it
 * also collects all land rent and spends it on the public (st_spend). */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "util.h"

/* Set when the turn must end at once (sent to jail, gone to the Poor
 * House). Only ever one game plays at a time, so a file variable is fine. */
static int stop_turn;

static void st_spend(Game *g);
static void land(Game *g, int p, int from, int n, int depth);

/* ---- random numbers: xorshift64*, seeded so tests repeat ---- */

int game_random(Game *g, int n)
{
    unsigned long long x = g->rng ? g->rng : 0x9E3779B97F4A7C15ULL;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    g->rng = (unsigned long)x;
    if (n <= 1) return 0;
    return (int)(((x * 0x2545F4914F6CDD1DULL) >> 33) % (unsigned long long)n);
}

/* Fisher-Yates shuffle of n ints with the game's own random numbers. */
static void shuffle(Game *g, int *a, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = game_random(g, i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

/* ---- the log ---- */

void game_log(Game *g, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g->log[g->log_count % LOG_LINES], LOG_LEN, fmt, ap);
    va_end(ap);
    g->log_count++;
    if (g->hooks.changed) g->hooks.changed(g);
}

/* See game.h: the i-th newest line of the ring of messages. */
const char *game_log_line(const Game *g, int i)
{
    if (i < 0 || i >= g->log_count || i >= LOG_LINES) return NULL;
    return g->log[(g->log_count - 1 - i) % LOG_LINES];
}

/* Player p's name, for messages. */
static const char *nm(const Game *g, int p) { return g->pl[p].name; }

/* Let the window show what happened (the hook may wait for the human to
 * watch). */
static void pause_here(Game *g, int p)
{
    if (g->hooks.pause) g->hooks.pause(g, p);
}

/* ---- setting up ---- */

void game_new(Game *g, const GameSettings *s)
{
    memset(g, 0, sizeof *g);
    g->rng = s->seed ? s->seed : 0x1904;
    g->nplayers = CLAMP(s->nplayers, 2, MAXPLAYERS);
    /* The 1906 rules: "if there are two players, each provides himself with
     * $600; if three players, each $500; if four players, each $400." */
    int start = g->nplayers == 2 ? 600 : g->nplayers == 3 ? 500 : 400;
    for (int p = 0; p < g->nplayers; p++) {
        Player *P = &g->pl[p];
        snprintf(P->name, sizeof P->name, "%s", s->names[p][0] ? s->names[p] : "Player");
        P->cpu = s->cpu[p] != 0;
        P->difficulty = s->difficulty[p];
        P->cash = start;
    }
    for (int i = 0; i < NSPACES; i++) { g->owner[i] = NOBODY; g->house_owner[i] = NOBODY; }
    g->wage = 100;
    g->trust_a = g->trust_b = -1;
    g->advanced = s->advanced;
    g->end_wages = s->end_wages > 0 ? s->end_wages : 5;
    for (int i = 0; i < nchance_cards && i < 32; i++) g->chance_deck[i] = i;
    shuffle(g, g->chance_deck, MIN(nchance_cards, 32));
}

void game_set_decider(Game *g, int p, const Decider *d) { g->dec[p] = d; }

/* ---- questions ---- */

int game_section(int sp)
{
    int off = sp % SIDE;
    if (off == 0 || off == SIDE / 2) return -1;     /* corners and railroads divide the sections */
    return (sp / SIDE) * 2 + (off > SIDE / 2);
}

/* Houses a lot may hold: three under the advanced rules, nine otherwise. */
static int house_limit(const Game *g) { return g->advanced ? 3 : 9; }

/* See game.h. Under the advanced rules the rent doubles for each round of
 * houses on every lot of the section. */
int game_land_rent(const Game *g, int sp)
{
    int rent = board[sp].rent;
    if (!g->advanced || board[sp].type != T_LOT) return rent;
    /* Advanced rules: when every lot of a section has at least one house the
     * land rent there doubles, at two houses doubles again, at three again. */
    int sec = game_section(sp), least = 99;
    for (int i = 0; i < NSPACES; i++)
        if (board[i].type == T_LOT && game_section(i) == sec) least = MIN(least, g->houses[i]);
    return rent << MIN(least, 3);
}

/* The railroads player p owns (not counting those taken over by the
 * public). */
static int rails_owned(const Game *g, int p)
{
    int n = 0;
    for (int i = 0; i < NSPACES; i++)
        if (board[i].type == T_RAILROAD && g->owner[i] == p && !g->public_owned[i]) n++;
    return n;
}

static int in_trust(const Game *g, int p) { return p >= 0 && (p == g->trust_a || p == g->trust_b); }

/* See game.h: $5 for one railroad, $10 / $20 / $50 with two, three, four
 * (MONOPOLY), $40 in a TRUST, nothing when publicly owned. */
int game_rail_fare(const Game *g, int sp)
{
    int o = g->owner[sp];
    if (g->public_owned[sp]) return 0;
    if (o == NOBODY) return 5;
    if (in_trust(g, o)) return 40;
    static const int fare[5] = { 0, 5, 10, 20, 50 };   /* MONOPOLY: 2, 3, 4 railroads */
    return fare[MIN(rails_owned(g, o), 4)];
}

/* A franchise: $5, or $25 when one owner holds both (the MUNICIPAL CINCH). */
static int franchise_fare(const Game *g, int sp)
{
    int o = g->owner[sp];
    if (g->public_owned[sp]) return 0;
    if (o == NOBODY) return 5;
    int both = g->owner[16] == o && g->owner[22] == o && !g->public_owned[16] && !g->public_owned[22];
    return both ? 25 : 5;                               /* MUNICIPAL CINCH */
}

/* Houses on the whole board (the advanced rules raise taxes at 10 and 25). */
int game_houses_total(const Game *g)
{
    int n = 0;
    for (int i = 0; i < NSPACES; i++) n += g->houses[i];
    return n;
}

/* See game.h: $10, $20 at 10 houses and $40 at 25 under the advanced rules,
 * nothing under the Single Tax. */
int game_tax(const Game *g)
{
    if (g->single_tax) return 0;
    if (g->advanced) {
        int h = game_houses_total(g);
        if (h >= 25) return 40;
        if (h >= 10) return 20;
    }
    return 10;
}

/* See game.h: cards player p holds, of one kind or all. */
int game_cards_of(const Game *g, int p, int type)
{
    int n = 0;
    for (int i = 0; i < NSPACES; i++)
        if (g->owner[i] == p && (type < 0 || (int)board[i].type == type)) n++;
    return n;
}

/* See game.h: houses player p has built. */
int game_houses_of(const Game *g, int p)
{
    int n = 0;
    for (int i = 0; i < NSPACES; i++)
        if (g->house_owner[i] == p) n += g->houses[i];
    return n;
}

/* See game.h: cash plus 100 for each card, house, luxury and Professor card
 * (no lot deeds under the Single Tax). */
int game_worth(const Game *g, int p)
{
    const Player *P = &g->pl[p];
    int w = P->cash + 100 * (game_houses_of(g, p) + P->luxuries + P->professors);
    for (int i = 0; i < NSPACES; i++) {
        if (g->owner[i] != p) continue;
        if (g->single_tax && board[i].type == T_LOT) continue;  /* "Title Deeds are of no value" */
        w += 100;
    }
    return w;
}

/* See game.h: the player of greatest worth (the first such on a tie). */
int game_leader(const Game *g)
{
    int best = 0;
    for (int p = 1; p < g->nplayers; p++)
        if (game_worth(g, p) > game_worth(g, best)) best = p;
    return best;
}

/* See game.h: an estimate for the computer players of what a stop on sp
 * costs p (negative = a gain). */
int game_landing_cost(const Game *g, int p, int sp)
{
    const Space *s = &board[sp];
    int o = g->owner[sp];
    switch (s->type) {
    case T_LOT:
        if (g->single_tax) {
            int c = game_land_rent(g, sp);
            if (g->houses[sp] && g->house_owner[sp] != p) c += 10 * g->houses[sp];
            return c;
        }
        if (o == NOBODY || o == p) return 0;
        return game_land_rent(g, sp) + 10 * g->houses[sp];
    case T_RAILROAD:
        if (o == NOBODY || o == p || (in_trust(g, p) && in_trust(g, o))) return 0;
        return game_rail_fare(g, sp);
    case T_FRANCHISE:
        if (o == NOBODY || o == p) return 0;
        return franchise_fare(g, sp);
    case T_TAX:         return game_tax(g);
    case T_TRESPASS:    return g->colleges[sp] ? -25 : 80;  /* the fine, or a lost turn */
    case T_CHANCE:      return 4;
    case T_SPECULATION: return -40;                         /* the broker's game pays on average */
    case T_LEGACY:      return -100;
    default:            return 0;
    }
}

/* ---- money ---- */

/* Move money that is certainly there. */
static void transfer(Game *g, int from, int to, int amount)
{
    if (amount <= 0) return;
    if (from >= 0) g->pl[from].cash -= amount;
    if (to >= 0) g->pl[to].cash += amount;
    else if (to == TO_TREASURY) {
        g->treasury += amount;
        if (g->single_tax) st_spend(g);
    }
}

static void collect(Game *g, int p, int amount) { g->pl[p].cash += amount; }

/* Has p anything left to sell to raise money? */
static int has_assets(const Game *g, int p)
{
    for (int i = 0; i < NSPACES; i++) {
        if (g->house_owner[i] == p && g->houses[i]) return 1;
        if (g->owner[i] == p && !(g->single_tax && board[i].type == T_LOT)) return 1;
    }
    return 0;
}

/* Rule 12: a player who cannot pay goes to the Poor House. Under the Single
 * Tax there is no Poor House (Single Tax Rule 8): he rests on the Natural
 * Opportunity (inner corner) behind him and draws wages from it next turn. */
static void broke(Game *g, int p)
{
    Player *P = &g->pl[p];
    P->in_jail = 0;
    if (g->single_tax) {
        P->pos = (P->pos / SIDE) * SIDE;
        P->natural_opp = 1;
        game_log(g, "%s rests on a Natural Opportunity (%s).", nm(g, p), board[P->pos].name);
    } else {
        P->pos = SP_POORHOUSE;
        P->in_poorhouse = 1;
        game_log(g, "%s goes to the Poor House.", nm(g, p));
    }
    stop_turn = 1;
}

/* p must pay `amount` to `to`. If he has too little he raises money by
 * selling (his Decider chooses what); failing that he pays what he has and
 * goes to the Poor House. Returns 1 if paid in full. */
static int charge(Game *g, int p, int amount, int to)
{
    Player *P = &g->pl[p];
    if (amount <= 0) return 1;
    for (int guard = 0; P->cash < amount && guard < 64; guard++) {
        if (!has_assets(g, p) || !g->dec[p]->raise(g, p, amount - P->cash)) break;
    }
    if (P->cash >= amount) {
        transfer(g, p, to, amount);
        return 1;
    }
    game_log(g, "%s cannot pay $%d and hands over his last $%d.", nm(g, p), amount, P->cash);
    transfer(g, p, to, P->cash);
    broke(g, p);
    return 0;
}

/* ---- the Single Tax ---- */

static void enact_single_tax(Game *g)
{
    g->single_tax = 1;
    g->treasury = 0;                    /* "Begin game under single tax with empty PUBLIC TREASURY." */
    for (int i = 0; i < NSPACES; i++)
        if (g->houses[i] && g->house_owner[i] == NOBODY) g->house_owner[i] = g->owner[i];
    game_log(g, "THE SINGLE TAX is in force: land rent now goes to the Public Treasury.");
}

/* Single Tax Rule 6: every $50 in the treasury buys a utility for the public,
 * then builds a free college, then raises wages by $10. */
static void st_spend(Game *g)
{
    while (g->treasury >= 50) {
        g->treasury -= 50;
        int k = g->st_progress++;
        int nutil = 0;
        while (public_order[nutil] >= 0) nutil++;
        if (k < nutil) {
            int sp = public_order[k];
            int o = g->owner[sp];
            if (o != NOBODY) {
                collect(g, o, 50);
                game_log(g, "The public buys %s from %s for $50; it is now free.", board[sp].name, nm(g, o));
            } else {
                game_log(g, "%s is taken over by the public; it is now free.", board[sp].name);
            }
            g->owner[sp] = NOBODY;
            g->public_owned[sp] = 1;
            if (in_trust(g, o)) g->trust_a = g->trust_b = -1;
        } else if (k == nutil) {
            g->colleges[13] = 1;
            game_log(g, "A FREE COLLEGE is built on Lord Blueblood's Estate.");
        } else if (k == nutil + 1) {
            g->colleges[29] = 1;
            game_log(g, "A FREE COLLEGE is built on D. F. Hogg's Game Preserves.");
        } else {
            g->wage += 10;
            game_log(g, "WAGES ARE RAISED to $%d.", g->wage);
        }
    }
}

/* See game.h: p proposes, the others vote; two votes in all carry it. */
int game_propose_single_tax(Game *g, int p)
{
    if (g->single_tax || g->over) return 0;
    int votes = 1;
    game_log(g, "%s proposes the Single Tax.", nm(g, p));
    for (int q = 0; q < g->nplayers; q++) {
        if (q == p) continue;
        int yes = g->dec[q]->vote_single_tax(g, q, p);
        game_log(g, "  %s votes %s.", nm(g, q), yes ? "for it" : "against it");
        votes += yes;
    }
    if (votes >= 2) {
        enact_single_tax(g);
        return 1;
    }
    game_log(g, "The Single Tax is defeated.");
    return 0;
}

/* ---- the TRUST ---- */

/* Rule 9: "If two players own all of the RAILROADS between them, they may at
 * any time pool their railroad interests and form a Trust." A trust breaks
 * up when a railroad changes hands. */
static void check_trust(Game *g)
{
    int a = -1, b = -1, ok = 1;
    for (int i = 0; i < NSPACES && ok; i++) {
        if (board[i].type != T_RAILROAD) continue;
        int o = g->owner[i];
        if (o == NOBODY || g->public_owned[i]) ok = 0;
        else if (a < 0 || o == a) a = o;
        else if (b < 0 || o == b) b = o;
        else ok = 0;
    }
    ok = ok && a >= 0 && b >= 0;
    if (g->trust_a >= 0 && !(ok && in_trust(g, a) && in_trust(g, b))) {
        game_log(g, "The railroad Trust is dissolved.");
        g->trust_a = g->trust_b = -1;
    }
    if (!ok) { g->trust_refused = 0; return; }
    if (g->trust_a >= 0 || g->trust_refused) return;
    if (g->dec[a]->join_trust(g, a, b) && g->dec[b]->join_trust(g, b, a)) {
        g->trust_a = a;
        g->trust_b = b;
        game_log(g, "%s and %s pool their railroads in a TRUST: fares are now $40.", nm(g, a), nm(g, b));
    } else {
        g->trust_refused = 1;
    }
}

/* ---- buying and selling ---- */

static void take_card(Game *g, int p, int sp, int price)
{
    transfer(g, p, TO_TREASURY, price);
    g->owner[sp] = p;
    game_log(g, "%s buys %s for $%d.", nm(g, p), board[sp].name, price);
    if (board[sp].type == T_RAILROAD) check_trust(g);
}

/* Rule 1: deal the cards round, at most 24, and let each player buy those
 * dealt to him. */
void game_deal(Game *g)
{
    int cards[NSPACES], n = 0;
    for (int i = 0; i < NSPACES; i++)
        if (board_has_card(i) && g->owner[i] == NOBODY && !g->public_owned[i]) cards[n++] = i;
    shuffle(g, cards, n);
    int deal = MIN(n, 24);              /* "deal out one around ... until 24 have been dealt" */
    game_log(g, "The cards are dealt: each player may buy those dealt to him.");
    for (int p = 0; p < g->nplayers; p++) {
        int mine[NSPACES], want[NSPACES], m = 0;
        for (int i = p; i < deal; i += g->nplayers) mine[m++] = cards[i];
        /* Show them in board order. */
        for (int i = 1; i < m; i++)
            for (int j = i; j > 0 && mine[j - 1] > mine[j]; j--) { int t = mine[j]; mine[j] = mine[j - 1]; mine[j - 1] = t; }
        memset(want, 0, sizeof want);
        g->dec[p]->deal_buy(g, p, mine, m, want);
        int bought = 0;
        for (int i = 0; i < m; i++) {
            if (!want[i] || g->pl[p].cash < board[mine[i]].price) continue;
            take_card(g, p, mine[i], board[mine[i]].price);
            bought++;
        }
        if (!bought) game_log(g, "%s buys none of his cards.", nm(g, p));
    }
    pause_here(g, -1);
}

/* Rule 7: p has landed on unowned sp. The other players may bid more than the
 * sale price; p has the first option at the highest price bid. Returns the
 * new owner, or NOBODY if nobody buys. Bidding runs like a real auction in $5
 * steps: the winner pays $5 over the runner-up's limit, never more than his
 * own limit. */
static int offer(Game *g, int p, int sp)
{
    int price = board[sp].price;
    int best = NOBODY, best_max = price, second = price;
    for (int q = 0; q < g->nplayers; q++) {
        if (q == p || g->pl[q].cash <= price) continue;
        int m = MIN(g->dec[q]->max_bid(g, q, sp, price + 5), g->pl[q].cash);
        if (m <= price) continue;
        if (m > best_max) { second = best_max; best_max = m; best = q; }
        else if (m > second) second = m;
    }
    int high = best == NOBODY ? price : MIN(best_max, MAX(second, price) + 5);
    if (best != NOBODY) game_log(g, "%s bids $%d for %s.", nm(g, best), high, board[sp].name);
    if (g->pl[p].cash >= high && g->dec[p]->buy(g, p, sp, high, best)) {
        take_card(g, p, sp, high);
        return p;
    }
    if (best != NOBODY) {
        take_card(g, best, sp, high);
        return best;
    }
    return NOBODY;
}

/* See game.h: sell card sp between two players; refused for lots with
 * houses (sell those first) and for lot deeds under the Single Tax. */
const char *game_trade(Game *g, int seller, int buyer, int sp, int price)
{
    if (!board_has_card(sp) || g->owner[sp] != seller) return "That card is not his to sell.";
    if (seller == buyer) return "One cannot trade with oneself.";
    if (board[sp].type == T_LOT && g->single_tax) return "Under the Single Tax title deeds are of no value.";
    if (g->houses[sp] && !g->single_tax) return "Sell the houses on that lot first.";
    if (price < 0 || g->pl[buyer].cash < price) return "The buyer has not the money.";
    transfer(g, buyer, seller, price);
    g->owner[sp] = buyer;
    game_log(g, "%s sells %s to %s for $%d.", nm(g, seller), board[sp].name, nm(g, buyer), price);
    if (board[sp].type == T_RAILROAD) check_trust(g);
    return NULL;
}

/* See game.h: back to the pack for half the sale price. */
const char *game_sell_to_pack(Game *g, int p, int sp)
{
    if (!board_has_card(sp) || g->owner[sp] != p) return "That card is not yours.";
    if (board[sp].type == T_LOT && g->single_tax) return "Under the Single Tax title deeds are of no value.";
    if (g->houses[sp] && !g->single_tax) return "Sell the houses on that lot first.";
    collect(g, p, PACK_PRICE(sp));
    g->owner[sp] = NOBODY;
    game_log(g, "%s sells %s back to the pack for $%d.", nm(g, p), board[sp].name, PACK_PRICE(sp));
    if (board[sp].type == T_RAILROAD) check_trust(g);
    return NULL;
}

/* See game.h: a house back to the bank for $50. */
const char *game_sell_house(Game *g, int p, int sp)
{
    if (g->houses[sp] <= 0 || g->house_owner[sp] != p) return "You have no house there.";
    g->houses[sp]--;
    if (!g->houses[sp]) g->house_owner[sp] = NOBODY;
    collect(g, p, HOUSE_RESALE);
    game_log(g, "%s sells a house on %s for $%d.", nm(g, p), board[sp].name, HOUSE_RESALE);
    return NULL;
}

/* See game.h: own lots only, or under the Single Tax any unimproved lot (or
 * one with your own houses); never past the house limit. */
int game_can_build(const Game *g, int p, int sp)
{
    if (sp < 0 || sp >= NSPACES || board[sp].type != T_LOT || g->over) return 0;
    if (g->houses[sp] >= house_limit(g)) return 0;
    if (g->single_tax) return g->houses[sp] == 0 || g->house_owner[sp] == p;
    return g->owner[sp] == p;
}

/* See game.h: build one house for $100. */
const char *game_build(Game *g, int p, int sp)
{
    if (!game_can_build(g, p, sp)) {
        if (sp >= 0 && sp < NSPACES && board[sp].type == T_LOT && g->houses[sp] >= house_limit(g))
            return "That lot already has as many houses as the rules allow.";
        return g->single_tax ? "Houses go on unimproved lots, or beside your own houses."
                             : "You can only build on your own lots.";
    }
    if (g->pl[p].cash < HOUSE_PRICE) return "A house costs $100.";
    transfer(g, p, TO_MISC, HOUSE_PRICE);
    g->houses[sp]++;
    g->house_owner[sp] = p;
    game_log(g, "%s builds a house on %s.", nm(g, p), board[sp].name);
    return NULL;
}

/* ---- moving ---- */

static void wages(Game *g, int p)
{
    Player *P = &g->pl[p];
    P->wages++;
    collect(g, p, g->wage);
    game_log(g, "%s passes Mother Earth and draws wages of $%d (%d of %d).",
             nm(g, p), g->wage, P->wages, g->end_wages);
}

/* Forward one space at a time: reaching or passing MOTHER EARTH pays wages. */
static void step_forward(Game *g, int p, int n)
{
    for (int i = 0; i < n; i++) {
        g->pl[p].pos = (g->pl[p].pos + 1) % NSPACES;
        if (g->pl[p].pos == SP_MOTHER_EARTH) wages(g, p);
    }
}

/* Put p in Jail (Rules 5 and 6) and end his turn. */
static void go_to_jail(Game *g, int p)
{
    Player *P = &g->pl[p];
    P->pos = SP_JAIL;
    P->in_jail = 1;
    P->jail_turns = 0;
    stop_turn = 1;
    game_log(g, "%s goes to Jail.", nm(g, p));
}

/* Throw two dice and show them. */
static void roll(Game *g, int *d1, int *d2)
{
    *d1 = 1 + game_random(g, 6);
    *d2 = 1 + game_random(g, 6);
    g->last_d1 = *d1;
    g->last_d2 = *d2;
    if (g->hooks.dice) g->hooks.dice(g, *d1, *d2);
}

/* Steps from `from` to the next corner ahead (10 when on a corner). */
static int to_corner(int from) { return SIDE - from % SIDE; }

/* Where a throw of n with the official pass would end. */
static int pass_dest(int from, int n) { return (from + n + SIDE - 1) % NSPACES; }

/* Move p by a throw of d1 and d2: the backward choice between the Chances,
 * the railroad pass on a double, the refusal to speculate; then settle the
 * space. */
static void move(Game *g, int p, int d1, int d2)
{
    Player *P = &g->pl[p];
    int n = d1 + d2, from = P->pos;
    int fwd = (from + n) % NSPACES, back = (from - n + NSPACES) % NSPACES;

    int forward = 1;
    if (from > CHANCE_A && from < CHANCE_B) {           /* Rule 2: between the Chances */
        forward = g->dec[p]->direction(g, p, fwd, back);
        if (!forward) game_log(g, "%s moves backward.", nm(g, p));
    }
    int pass = 0;
    if (forward && d1 == d2 && n > to_corner(from)) {   /* Rule 17: the official pass */
        pass = g->dec[p]->use_pass(g, p, pass_dest(from, n), fwd);
        if (pass) game_log(g, "%s uses his railroad pass.", nm(g, p));
    }
    int dest = !forward ? back : pass ? pass_dest(from, n) : fwd;

    /* Rule 8: one may refuse to move onto SPECULATION. */
    if (board[dest].type == T_SPECULATION && !g->dec[p]->speculate(g, p)) {
        game_log(g, "%s will not speculate and stays where he is.", nm(g, p));
        return;
    }
    if (!forward) {
        P->pos = back;
    } else if (pass) {
        int c = to_corner(from);
        step_forward(g, p, c);                          /* to the corner */
        step_forward(g, p, SIDE);                       /* jump to the next corner */
        step_forward(g, p, n - c - 1);
    } else {
        step_forward(g, p, n);
    }
    game_log(g, "%s throws %d and %d: %s.", nm(g, p), d1, d2, board[P->pos].name);
    land(g, p, from, n, 0);
}

/* ---- settling a space ---- */

/* What p owes landing on a card's space owned by someone else. */
static void pay_owner(Game *g, int p, int sp, int o)
{
    const Space *s = &board[sp];
    int amount;
    if (o < 0 || o >= g->nplayers) return;
    if (s->type == T_LOT) amount = game_land_rent(g, sp) + 10 * g->houses[sp];
    else if (s->type == T_RAILROAD) amount = game_rail_fare(g, sp);
    else amount = franchise_fare(g, sp);

    if (s->type == T_RAILROAD && in_trust(g, o)) {
        if (in_trust(g, p)) { game_log(g, "%s rides free on the Trust's line.", nm(g, p)); return; }
        game_log(g, "%s pays the Trust fare of $%d.", nm(g, p), amount);
        int a = amount / 2, b = amount - a;         /* the partners divide the profits */
        if (charge(g, p, a, g->trust_a)) charge(g, p, b, g->trust_b);
        return;
    }
    game_log(g, "%s pays $%d %s to %s.", nm(g, p), amount,
             s->type == T_LOT ? "rent" : "fare", nm(g, o));
    charge(g, p, amount, o);
}

/* Lots, railroads and franchises. */
static void property(Game *g, int p, int sp)
{
    const Space *s = &board[sp];
    if (g->public_owned[sp]) {
        game_log(g, "%s is owned by the public: no charge.", s->name);
        return;
    }
    if (s->type == T_LOT && g->single_tax) {
        /* Single Tax Rule 4: land rent to the treasury, house rent to the
         * builder; an unimproved lot may be built on. */
        int rent = game_land_rent(g, sp);
        game_log(g, "%s pays $%d land rent into the Public Treasury.", nm(g, p), rent);
        if (!charge(g, p, rent, TO_TREASURY)) return;
        int ho = g->house_owner[sp];
        if (g->houses[sp] && ho != p && ho != NOBODY) {
            game_log(g, "%s pays $%d house rent to %s.", nm(g, p), 10 * g->houses[sp], nm(g, ho));
            if (!charge(g, p, 10 * g->houses[sp], ho)) return;
        }
        if (!g->houses[sp] && g->pl[p].cash >= HOUSE_PRICE && g->dec[p]->st_build(g, p, sp))
            game_build(g, p, sp);
        return;
    }
    int o = g->owner[sp];
    if (o == p) return;
    if (o == NOBODY) {
        o = offer(g, p, sp);
        if (o == p) return;
        if (o == NOBODY) {
            int amount = s->type == T_LOT ? game_land_rent(g, sp) : 5;
            game_log(g, "Nobody buys %s; %s pays $%d into the Public Treasury.", s->name, nm(g, p), amount);
            charge(g, p, amount, TO_TREASURY);
            return;
        }
    }
    pay_owner(g, p, sp, o);
}

/* Rule 8: SPECULATION. */
static void speculation(Game *g, int p)
{
    int sp = SP_PARK + 1;
    int broker = g->owner[sp];
    if (broker == NOBODY) broker = offer(g, p, sp);
    int to = broker == NOBODY ? TO_MISC : broker;
    if (broker != p) {
        game_log(g, "%s pays the $10 ante%s%s.", nm(g, p), broker == NOBODY ? "" : " to the broker, ",
                 broker == NOBODY ? "" : nm(g, broker));
        if (!charge(g, p, 10, to)) return;
    }
    int d1, d2;
    roll(g, &d1, &d2);
    int sum = d1 + d2, win;
    if (d1 == d2) win = 100;
    else if (sum == 3 || sum == 4) win = 0;
    else win = (sum - 2) * 10;          /* 11: $90, 10: $80 ... 5: $30 */
    if (!win) {
        game_log(g, "%s throws %d: a \"skin game\"! The speculator wins nothing.", nm(g, p), sum);
        if (broker != NOBODY) {
            game_log(g, "The broker, %s, is caught; the license goes back to the pack.", nm(g, broker));
            g->owner[sp] = NOBODY;
            if (broker == p) stop_turn = 1;
            Player *B = &g->pl[broker];
            B->pos = SP_JAIL; B->in_jail = 1; B->jail_turns = 0;
            game_log(g, "%s goes to Jail.", nm(g, broker));
        }
        return;
    }
    int cut = broker != NOBODY && broker != p ? win / 10 : 0;
    collect(g, p, win - cut);
    if (cut) collect(g, broker, cut);
    game_log(g, "%s throws %d%s and wins $%d%s.", nm(g, p), sum, d1 == d2 ? " (a double)" : "", win,
             cut ? ", paying 10% to the broker" : "");
}

/* Rule 11: draw a red card and do what it says (moves settle their new
 * space). */
static void draw_chance(Game *g, int p, int depth)
{
    Player *P = &g->pl[p];
    int n = MIN(nchance_cards, 32);
    if (g->chance_next >= n) { g->chance_next = 0; shuffle(g, g->chance_deck, n); }
    const Chance *c = &chance_cards[g->chance_deck[g->chance_next++]];
    game_log(g, "Chance: %s", c->text);
    if (g->hooks.chance) g->hooks.chance(g, p, c->text);
    int from = P->pos;
    switch (c->kind) {
    case CH_COLLECT: collect(g, p, c->amount); break;
    case CH_PAY:     charge(g, p, c->amount, TO_TREASURY); break;
    case CH_JAIL:    go_to_jail(g, p); break;
    case CH_GOTO:
        while (P->pos != c->amount) step_forward(g, p, 1);
        land(g, p, from, 0, depth + 1);
        break;
    case CH_NEXT_RR:
        do step_forward(g, p, 1); while (board[P->pos].type != T_RAILROAD);
        game_log(g, "%s rides to %s.", nm(g, p), board[P->pos].name);
        land(g, p, from, 0, depth + 1);
        break;
    case CH_BACK:
        P->pos = (P->pos - c->amount + NSPACES) % NSPACES;
        game_log(g, "%s goes back to %s.", nm(g, p), board[P->pos].name);
        land(g, p, from, 0, depth + 1);
        break;
    case CH_PER_HOUSE: charge(g, p, c->amount * game_houses_of(g, p), TO_TREASURY); break;
    case CH_PER_LOT:   collect(g, p, c->amount * game_cards_of(g, p, T_LOT)); break;
    case CH_FROM_EACH:
        for (int q = 0; q < g->nplayers; q++) if (q != p) charge(g, q, c->amount, p);
        stop_turn = 0;              /* another player's trouble does not end this turn */
        break;
    case CH_TO_EACH:
        for (int q = 0; q < g->nplayers && !stop_turn; q++) if (q != p) charge(g, p, c->amount, q);
        break;
    }
}

/* Settle the space p has stopped on. `from` and `n` are where the move
 * started and its length (for LUXURY's backward move). */
static void land(Game *g, int p, int from, int n, int depth)
{
    Player *P = &g->pl[p];
    int sp = P->pos;
    const Space *s = &board[sp];
    if (depth > 4) return;                      /* (a chain of cards cannot go on forever) */
    switch (s->type) {
    case T_MOTHER: case T_PARK: case T_JAIL: case T_POOR:
        break;
    case T_LOT: case T_RAILROAD: case T_FRANCHISE:
        property(g, p, sp);
        break;
    case T_TAX:
        if (g->single_tax) { game_log(g, "Under the Single Tax there is no tax on %s.", s->name); break; }
        game_log(g, "%s pays $%d tax on %s.", nm(g, p), game_tax(g), s->name);
        charge(g, p, game_tax(g), TO_TREASURY);
        break;
    case T_TRESPASS:
        if (g->colleges[sp]) {
            P->education++;
            game_log(g, "%s goes to the Free College and earns an Education card.", nm(g, p));
            if (P->education >= 4) {
                P->education -= 4;
                P->professors++;
                game_log(g, "%s trades four Education cards for a PROFESSOR card.", nm(g, p));
            }
        } else {
            game_log(g, "%s is trespassing on %s!", nm(g, p), s->name);
            go_to_jail(g, p);
        }
        break;
    case T_CHANCE:
        draw_chance(g, p, depth);
        break;
    case T_LUXURY:
        if (P->cash >= 75 && g->dec[p]->luxury(g, p)) {
            transfer(g, p, TO_MISC, 75);
            P->luxuries++;
            game_log(g, "%s buys a luxury for $75 (it counts 100 at the end).", nm(g, p));
        } else if (n > 0 && depth == 0) {
            P->pos = (from - n + NSPACES) % NSPACES;
            game_log(g, "%s does without the luxury and moves backward to %s.", nm(g, p), board[P->pos].name);
            land(g, p, from, 0, depth + 1);
        }
        break;
    case T_SPECULATION:
        speculation(g, p);
        break;
    case T_LEGACY:
        collect(g, p, 100);
        game_log(g, "%s receives a legacy of $100.", nm(g, p));
        break;
    }
}

/* ---- the turn ---- */

void game_turn(Game *g)
{
    if (g->over) return;
    int p = g->cur;
    Player *P = &g->pl[p];
    stop_turn = 0;

    if (P->in_poorhouse) {
        P->in_poorhouse = 0;
        game_log(g, "%s leaves the Poor House.", nm(g, p));
    }
    if (P->natural_opp) {
        P->natural_opp = 0;
        collect(g, p, 50);
        game_log(g, "%s draws $50 wages from the Natural Opportunity.", nm(g, p));
    }
    check_trust(g);
    if (g->dec[p]->pre_roll) g->dec[p]->pre_roll(g, p);

    int d1, d2;
    if (P->in_jail) {
        P->jail_turns++;
        int must = P->jail_turns >= 3;  /* "He cannot ... miss more than three turns" */
        int can = P->cash >= 50 || has_assets(g, p);
        if (can && (must || g->dec[p]->jail_pay(g, p))) {
            game_log(g, "%s pays the $50 fine and leaves Jail.", nm(g, p));
            if (!charge(g, p, 50, TO_TREASURY)) goto done;
            P->in_jail = 0;
            roll(g, &d1, &d2);
        } else {
            roll(g, &d1, &d2);
            if (d1 != d2) {
                game_log(g, "%s throws %d and %d and stays in Jail.", nm(g, p), d1, d2);
                goto done;
            }
            game_log(g, "%s throws a double and has served his time.", nm(g, p));
            P->in_jail = 0;
        }
    } else {
        roll(g, &d1, &d2);
    }
    move(g, p, d1, d2);

done:
    g->turn_no++;
    for (int q = 0; q < g->nplayers; q++) {
        if (g->pl[q].wages >= g->end_wages) {
            g->over = 1;
            game_log(g, "%s has received his wages %d times: the game is over.", nm(g, q), g->end_wages);
            break;
        }
    }
    pause_here(g, p);
    if (!g->over) g->cur = (g->cur + 1) % g->nplayers;
}
