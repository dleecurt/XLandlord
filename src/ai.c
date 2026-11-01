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

/* ai.c - see ai.h.
 *
 * The arithmetic the HARD player uses:
 *   - A checker moves 7 spaces a throw on average, so in one trip round the
 *     40-space board it stops on about 40 / 7 = 5.7 spaces: any one space is
 *     stopped on about once in 7 circuits (LAND_RATE) by each opponent.
 *   - The game lasts until somebody draws wages end_wages times, so the
 *     circuits left are end_wages minus the most wages drawn so far.
 *   - A card counts 100 at the end, whatever it cost, so its worth is 100
 *     plus the rent it will bring in: rent x opponents x LAND_RATE x circuits.
 *   - The money to keep in hand is what one circuit is likely to cost:
 *     the sum over the board of (what stopping there costs) x LAND_RATE. */
#include <string.h>

#include "ai.h"
#include "util.h"

#define LAND_RATE (1.0 / 7.0)

/* The level (DIFF_*) of computer player p. */
static int diff(const Game *g, int p) { return g->pl[p].difficulty; }
static int chance_pct(Game *g, int pct) { return game_random(g, 100) < pct; }

/* Trips round the board left before someone draws his last wages (at least
 * 0.75, so a card is never worth nothing). */
static double circuits_left(const Game *g)
{
    int most = 0;
    for (int q = 0; q < g->nplayers; q++) if (game_active(g, q)) most = MAX(most, g->pl[q].wages);
    return MAX(0.75, (double)(g->end_wages - most));
}

/* The railroads player p owns that the public has not taken over. */
static int rails_of(const Game *g, int p)
{
    int n = 0;
    for (int i = 0; i < NSPACES; i++)
        if (board[i].type == T_RAILROAD && g->owner[i] == p && !g->public_owned[i]) n++;
    return n;
}

static int rail_income(int count)       /* fares from `count` railroads, one stop on each */
{
    static const int fare[5] = { 0, 5, 10, 20, 50 };
    count = CLAMP(count, 0, 4);
    return count * fare[count];
}

/* How many cards are still in the pack (lots, charters, the broker's
 * license). */
static int unowned_cards(const Game *g)
{
    int n = 0;
    for (int i = 0; i < NSPACES; i++)
        if (board_has_card(i) && g->owner[i] == NOBODY && !g->public_owned[i]) n++;
    return n;
}

/* Extra income per circuit that owning sp would bring p (not counting a card
 * p already holds as extra). */
static double income_per_circuit(const Game *g, int p, int sp)
{
    int opp = MAX(1, game_active_count(g) - 1);     /* the others still at the table */
    switch (board[sp].type) {
    case T_LOT:
        if (g->single_tax) return 0;
        return (game_land_rent(g, sp) + 10.0 * g->houses[sp]) * opp * LAND_RATE;
    case T_RAILROAD: {
        int have = rails_of(g, p) - (g->owner[sp] == p);
        double gain = rail_income(have + 1) - rail_income(have);
        return gain * opp * LAND_RATE;
    }
    case T_FRANCHISE: {
        int other = sp == 16 ? 22 : 16;
        int pair = g->owner[other] == p && !g->public_owned[other];
        return (pair ? 45.0 : 5.0) * opp * LAND_RATE;   /* the Cinch turns $5 + $5 into $25 + $25 */
    }
    case T_SPECULATION:
        return 15.0 * opp * LAND_RATE;                  /* the ante plus a tenth of the winnings */
    default:
        return 0;
    }
}

/* What keeping sp out of an opponent's hands is worth (HARD only): the
 * railroad monopoly or the Municipal Cinch it would give him. */
static double blocking_value(const Game *g, int p, int sp)
{
    double best = 0;
    for (int q = 0; q < g->nplayers; q++) {
        if (q == p || !game_active(g, q)) continue;
        if (board[sp].type == T_RAILROAD) {
            int have = rails_of(g, q);
            if (have >= 2) best = MAX(best, (rail_income(have + 1) - rail_income(have)) * LAND_RATE * 0.5);
        } else if (board[sp].type == T_FRANCHISE) {
            int other = sp == 16 ? 22 : 16;
            if (g->owner[other] == q) best = MAX(best, 45.0 * LAND_RATE * 0.5);
        }
    }
    return best;
}

/* See ai.h. Easy guesses, Medium counts most of the end value and some
 * rent, Hard counts the end value, the expected rent for the circuits left,
 * and what keeping the card from a rival is worth. */
int ai_card_value(Game *g, int p, int sp)
{
    if (!board_has_card(sp)) return 0;
    double end = g->single_tax && board[sp].type == T_LOT ? 0 : 100;
    double c = circuits_left(g);
    switch (diff(g, p)) {
    case DIFF_EASY:
        return board[sp].price + game_random(g, 30);
    case DIFF_MEDIUM:
        return (int)(end * 0.8 + income_per_circuit(g, p, sp) * c * 0.6);
    default:
        /* (100 counted at the end is worth a little less than 100 in hand:
         * cash pays the rent on the way.) */
        return (int)(end * 0.75 + (income_per_circuit(g, p, sp) + blocking_value(g, p, sp)) * c);
    }
}

/* See ai.h. Easy and Medium keep a fixed sum; Hard keeps what a trip round
 * the board is expected to cost, or at least the dearest single stop. */
int ai_buffer(Game *g, int p)
{
    switch (diff(g, p)) {
    case DIFF_EASY:   return 30;
    case DIFF_MEDIUM: return 100;
    default: break;
    }
    double cost = 0;
    for (int i = 0; i < NSPACES; i++) cost += MAX(0, game_landing_cost(g, p, i)) * LAND_RATE;
    /* Railroad monopolies and trusts make some stops very dear: keep enough
     * for the dearest single stop too. */
    int worst = 0;
    for (int i = 0; i < NSPACES; i++) worst = MAX(worst, game_landing_cost(g, p, i));
    return CLAMP((int)MAX(cost, (double)worst), 40, 300);
}

/* ---- Rule 1 ---- */

static void deal_buy(Game *g, int p, const int *cards, int n, int *want)
{
    int cash = g->pl[p].cash;
    int d = diff(g, p);
    if (d == DIFF_EASY) {
        for (int i = 0; i < n; i++)
            if (cash - board[cards[i]].price >= 100 && chance_pct(g, 55)) { want[i] = 1; cash -= board[cards[i]].price; }
        return;
    }
    /* Best bargains first: worth minus price. */
    int order[NSPACES];
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int a = order[j - 1], b = order[j];
            if (ai_card_value(g, p, cards[a]) - board[cards[a]].price >=
                ai_card_value(g, p, cards[b]) - board[cards[b]].price) break;
            order[j - 1] = b; order[j] = a;
        }
    /* "one generally needs between $50 and $75 to pay expenses around the
     * board" - the 1906 rules' own advice. */
    int keep = d == DIFF_HARD ? 75 : 150;
    for (int k = 0; k < n; k++) {
        int i = order[k], price = board[cards[i]].price;
        if (cash - price >= keep && ai_card_value(g, p, cards[i]) >= price) { want[i] = 1; cash -= price; }
    }
}

/* ---- buying, bidding ---- */

static int ai_buy(Game *g, int p, int sp, int price, int high_bidder)
{
    (void)high_bidder;
    int cash = g->pl[p].cash;
    switch (diff(g, p)) {
    case DIFF_EASY:   return cash - price >= 30 && chance_pct(g, 70);
    case DIFF_MEDIUM: return cash - price >= 100 && ai_card_value(g, p, sp) >= price;
    default:          return cash - price >= ai_buffer(g, p) * 6 / 10 && ai_card_value(g, p, sp) >= price;
    }
}

/* Rule 7: the most p bids for unowned sp (0 = none). Hard bids up to the
 * card's worth to it, keeping its cash buffer. */
static int ai_max_bid(Game *g, int p, int sp, int min)
{
    int cash = g->pl[p].cash, v = ai_card_value(g, p, sp);
    switch (diff(g, p)) {
    case DIFF_EASY:   return cash - min >= 150 && chance_pct(g, 15) ? min : 0;
    case DIFF_MEDIUM: return MIN(board[sp].price + (v - board[sp].price) / 2, cash - 100);
    default:          return MIN(v, cash - ai_buffer(g, p));
    }
}

/* See ai.h: what p would pay another player for card sp. */
int ai_offer_for(Game *g, int p, int sp)
{
    int v = ai_card_value(g, p, sp), cash = g->pl[p].cash - ai_buffer(g, p);
    switch (diff(g, p)) {
    case DIFF_EASY:   return MAX(0, MIN(board[sp].price, cash));
    case DIFF_MEDIUM: return MAX(0, MIN(v * 9 / 10, cash));
    default:          return MAX(0, MIN(v - 5, cash));
    }
}

/* See ai.h: p sells only for more than the card is worth to it (Easy: a
 * little over the sale price). */
int ai_accepts_sale(Game *g, int p, int sp, int price)
{
    /* What the card is worth to p if it keeps it. */
    int v = ai_card_value(g, p, sp);
    switch (diff(g, p)) {
    case DIFF_EASY:   return price >= board[sp].price + 10;
    case DIFF_MEDIUM: return price >= v + 10;
    default:          return price >= v + 25;
    }
}

/* ---- choices on the way round ---- */

/* How bad stopping at sp would be for p (negative = good). The HARD player
 * counts a card it could buy as a gain and values progress towards wages. */
static int stop_cost(Game *g, int p, int sp)
{
    int c = game_landing_cost(g, p, sp);
    if (board_has_card(sp) && g->owner[sp] == NOBODY && !g->public_owned[sp] &&
        !(g->single_tax && board[sp].type == T_LOT) &&
        g->pl[p].cash - board[sp].price >= ai_buffer(g, p) / 2)
        c -= ai_card_value(g, p, sp) - board[sp].price;
    if (board[sp].type == T_LUXURY && g->pl[p].cash - 75 >= ai_buffer(g, p)) c -= 25;
    return c;
}

/* What one space of progress is worth to the HARD player: a fortieth of a
 * wage, and in the last circuit more, because the first to draw his last
 * wages ends the game - welcome when leading, ruinous when behind. */
static double pace(Game *g, int p)
{
    double v = g->wage / 40.0;
    if (g->pl[p].wages >= g->end_wages - 1) v += game_leader(g) == p ? 2 : -2;
    return v;
}

/* Rule 17: use the railroad pass? Hard weighs both landing squares and the
 * nine spaces of progress. */
static int ai_use_pass(Game *g, int p, int with, int without)
{
    switch (diff(g, p)) {
    case DIFF_EASY:   return chance_pct(g, 50);
    case DIFF_MEDIUM: return game_landing_cost(g, p, with) <= game_landing_cost(g, p, without) + 5;
    default:
        /* The pass carries the checker nine spaces further. */
        return stop_cost(g, p, with) - pace(g, p) * 9 <= stop_cost(g, p, without);
    }
}

/* Rule 2: forward (1) or backward (0) between the Chances, by the cost of
 * each landing square. */
static int ai_direction(Game *g, int p, int fwd, int back)
{
    switch (diff(g, p)) {
    case DIFF_EASY:   return chance_pct(g, 60);
    case DIFF_MEDIUM: return game_landing_cost(g, p, fwd) <= game_landing_cost(g, p, back) + 5;
    default: {
        int n = (fwd - g->pl[p].pos + NSPACES) % NSPACES;
        return stop_cost(g, p, fwd) - pace(g, p) * 2 * n <= stop_cost(g, p, back);
    }
    }
}

/* Rule 8: move onto Speculation? Its average payout beats the ante, so the
 * sensible players always play when they can. */
static int ai_speculate(Game *g, int p)
{
    /* On average the broker's game pays the speculator about $40 for a $10
     * ante, so the sensible players always play while they can ante. */
    if (diff(g, p) == DIFF_EASY) return chance_pct(g, 70);
    return g->pl[p].cash >= 10;
}

/* Rule 13: buy the luxury ($75 for a card worth 100) or move backward? */
static int ai_luxury(Game *g, int p)
{
    int cash = g->pl[p].cash;
    switch (diff(g, p)) {
    case DIFF_EASY:   return chance_pct(g, 50);
    case DIFF_MEDIUM: return cash >= 200;
    default:          return cash - 75 >= ai_buffer(g, p);    /* pays 75, counts 100 */
    }
}

/* Expected cost of a throw out of Jail. */
static double jail_exit_cost(Game *g, int p)
{
    double sum = 0;
    for (int a = 1; a <= 6; a++)
        for (int b = 1; b <= 6; b++) sum += stop_cost(g, p, (SP_JAIL + a + b) % NSPACES);
    return sum / 36.0;
}

/* Rule 6: pay the $50 fine (1) or try for a double (0)? */
static int ai_jail_pay(Game *g, int p)
{
    int cash = g->pl[p].cash;
    switch (diff(g, p)) {
    case DIFF_EASY:   return cash >= 50;
    case DIFF_MEDIUM: return cash >= 150 && circuits_left(g) > 2;
    default:
        if (cash - 50 < ai_buffer(g, p)) return 0;
        if (unowned_cards(g) >= 3) return 1;        /* still cards to buy: get moving */
        /* Late in the game the board is expensive and Jail is safe; the fine
         * is worth paying only if the way out is cheap. */
        return jail_exit_cost(g, p) < 10 && circuits_left(g) > 1.5;
    }
}

/* Single Tax Rule 4: build a house on the unimproved lot just landed on? */
static int ai_st_build(Game *g, int p, int sp)
{
    (void)sp;
    int cash = g->pl[p].cash;
    switch (diff(g, p)) {
    case DIFF_EASY:   return cash >= 150 && chance_pct(g, 30);
    case DIFF_MEDIUM: return cash >= 250;
    default:          return cash - HOUSE_PRICE >= ai_buffer(g, p);
    }
}

/* ---- raising money ---- */

/* Sell one thing. The HARD player sells what costs it least: a card to
 * another player who pays at least its worth, then houses and cards to the
 * pack by smallest loss; the others sell the cheapest card first. */
static int ai_raise(Game *g, int p, int need)
{
    (void)need;
    int best_sp = -1, best_kind = 0, best_buyer = -1, best_price = 0;
    double best_loss = 1e9;
    int d = diff(g, p);
    for (int sp = 0; sp < NSPACES; sp++) {
        if (g->house_owner[sp] == p && g->houses[sp]) {
            double loss = 100 - HOUSE_RESALE + (d == DIFF_HARD ? 10.0 * (g->nplayers - 1) * LAND_RATE * circuits_left(g) : 0);
            if (d != DIFF_HARD) loss += 1000;           /* the simple players sell houses last */
            if (loss < best_loss) { best_loss = loss; best_sp = sp; best_kind = 1; }
        }
        if (g->owner[sp] != p || (g->single_tax && board[sp].type == T_LOT)) continue;
        if (g->houses[sp] && !g->single_tax) continue;
        int keep = ai_card_value(g, p, sp);
        for (int q = 0; q < g->nplayers; q++) {         /* another computer player buying it */
            if (q == p || !g->pl[q].cpu || !game_active(g, q)) continue;
            int offer = ai_offer_for(g, q, sp);
            if (offer <= 0) continue;
            double loss = d == DIFF_HARD ? keep - offer : board[sp].price - offer;
            if (loss < best_loss) { best_loss = loss; best_sp = sp; best_kind = 2; best_buyer = q; best_price = offer; }
        }
        double loss = d == DIFF_HARD ? keep - PACK_PRICE(sp) : board[sp].price;
        if (loss < best_loss) { best_loss = loss; best_sp = sp; best_kind = 3; }
    }
    if (best_sp < 0) return 0;
    if (best_kind == 1) return game_sell_house(g, p, best_sp) == NULL;
    if (best_kind == 2) return game_trade(g, p, best_buyer, best_sp, best_price) == NULL;
    return game_sell_to_pack(g, p, best_sp) == NULL;
}

/* ---- the Single Tax and the Trust ---- */

/* p's distance behind the leader, now and as it would be under the Single
 * Tax (when lot title deeds count nothing). */
static void standing(const Game *g, int p, int *now, int *st)
{
    int best_now = -(1 << 30), best_st = -(1 << 30), me_now = 0, me_st = 0;
    for (int q = 0; q < g->nplayers; q++) {
        if (!game_active(g, q)) continue;
        int w = game_worth(g, q);
        int ws = w - (g->single_tax ? 0 : 100 * game_cards_of(g, q, T_LOT));
        if (q == p) { me_now = w; me_st = ws; continue; }
        best_now = MAX(best_now, w);
        best_st = MAX(best_st, ws);
    }
    *now = best_now - me_now;
    *st = best_st - me_st;
}

/* Vote on the Single Tax: Hard votes for it when it closes the gap to the
 * leader. */
static int ai_vote(Game *g, int p, int proposer)
{
    (void)proposer;
    int now, st;
    standing(g, p, &now, &st);
    switch (diff(g, p)) {
    case DIFF_EASY:   return chance_pct(g, 50);
    case DIFF_MEDIUM: return game_leader(g) != p;
    default:          return st < now;      /* yes if it closes the gap to the leader */
    }
}

/* Rule 9: join a railroad Trust? Always worth it, except to the impulsive
 * Easy player. */
static int ai_join_trust(Game *g, int p, int other)
{
    (void)other;
    return diff(g, p) == DIFF_EASY ? chance_pct(g, 50) : 1;
}

/* ---- before the throw ---- */

/* The lot where one more house helps most: under the advanced rules a house
 * that completes a section doubles the land rent of the whole section. */
static int best_lot_to_build(Game *g, int p)
{
    int best = -1, best_score = -1;
    for (int sp = 0; sp < NSPACES; sp++) {
        if (!game_can_build(g, p, sp)) continue;
        int score = game_land_rent(g, sp) + 10;
        if (g->advanced) {
            int sec = game_section(sp), least = 99, others = 0;
            for (int i = 0; i < NSPACES; i++) {
                if (board[i].type != T_LOT || game_section(i) != sec) continue;
                if (i != sp) { least = MIN(least, g->houses[i]); others++; }
            }
            int all_mine = 1;
            for (int i = 0; i < NSPACES; i++)
                if (board[i].type == T_LOT && game_section(i) == sec && g->owner[i] != p) all_mine = 0;
            if (all_mine && (!others || least > g->houses[sp])) score += 40;   /* completes a level */
        }
        if (score > best_score) { best_score = score; best = sp; }
    }
    return best;
}

/* Before the throw: build houses while the cash allows (Hard keeps its
 * buffer plus $150). */
static void build_houses(Game *g, int p)
{
    int d = diff(g, p);
    for (int k = 0; k < 6; k++) {
        int cash = g->pl[p].cash;
        int ok = d == DIFF_EASY ? cash >= 200 && chance_pct(g, 15)
               : d == DIFF_MEDIUM ? cash >= 300
               : cash - HOUSE_PRICE >= ai_buffer(g, p) + 150;
        if (!ok) return;
        int sp = best_lot_to_build(g, p);
        if (sp < 0 || game_build(g, p, sp)) return;
        if (d == DIFF_EASY) return;
    }
}

/* HARD: buy railroads and franchises from the other computer players when it
 * completes a monopoly or the Cinch. */
static void trade_for_monopolies(Game *g, int p)
{
    for (int sp = 0; sp < NSPACES; sp++) {
        int o = g->owner[sp];
        if (o == NOBODY || o == p || !g->pl[o].cpu || g->public_owned[sp]) continue;
        if (board[sp].type != T_RAILROAD && board[sp].type != T_FRANCHISE) continue;
        int mine = board[sp].type == T_RAILROAD ? rails_of(g, p) >= 2
                                               : g->owner[sp == 16 ? 22 : 16] == p;
        if (!mine) continue;
        int offer = ai_offer_for(g, p, sp);
        if (offer > 0 && ai_accepts_sale(g, o, sp, offer)) game_trade(g, o, p, sp, offer);
    }
}

static int last_proposal[MAXPLAYERS];

/* Before the throw: propose the Single Tax if it would help this player
 * (never more than once every few rounds). */
static void maybe_propose(Game *g, int p)
{
    if (g->single_tax || g->turn_no - last_proposal[p] < 6 * g->nplayers) return;
    if (g->turn_no < 4 * g->nplayers) return;               /* let the game get going first */
    int d = diff(g, p), now, st, go = 0;
    standing(g, p, &now, &st);
    if (d == DIFF_HARD) go = now > 0 && st < now - 50;
    else if (d == DIFF_MEDIUM) {
        int poorest = 1;
        for (int q = 0; q < g->nplayers; q++) if (game_active(g, q) && game_worth(g, q) < game_worth(g, p)) poorest = 0;
        go = poorest && now > 150;
    }
    if (!go) return;
    last_proposal[p] = g->turn_no;
    game_propose_single_tax(g, p);
}

/* The computer player's business before it throws: trades (Hard), houses,
 * the Single Tax. */
static void pre_roll(Game *g, int p)
{
    if (g->turn_no < g->nplayers) memset(last_proposal, 0, sizeof last_proposal);
    if (diff(g, p) == DIFF_HARD) trade_for_monopolies(g, p);
    build_houses(g, p);
    maybe_propose(g, p);
}

const Decider ai_decider = {
    .deal_buy = deal_buy,
    .buy = ai_buy,
    .max_bid = ai_max_bid,
    .jail_pay = ai_jail_pay,
    .use_pass = ai_use_pass,
    .direction = ai_direction,
    .speculate = ai_speculate,
    .luxury = ai_luxury,
    .st_build = ai_st_build,
    .raise = ai_raise,
    .vote_single_tax = ai_vote,
    .join_trust = ai_join_trust,
    .pre_roll = pre_roll,
};
