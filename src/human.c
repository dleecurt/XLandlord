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

/* human.c - see human.h. */
#include <stdio.h>
#include <string.h>

#include "human.h"
#include "ai.h"
#include "app.h"
#include "ui.h"
#include "util.h"
#include "view.h"
#include "xwin.h"

/* Player p's name, for messages. */
static const char *nm(const Game *g, int p) { return g->pl[p].name; }

/* Humans sharing this screen (remote players have their own). */
static int humans(const Game *g)
{
    (void)g;
    return app_local_humans();
}

/* With several humans at one screen every dialog says whom it is asking:
 * "Lee: For Sale (Rule 7)". Four buffers, so a few can be alive at once. */
static const char *for_player(const Game *g, int p, const char *title)
{
    static char buf[4][128];
    static int k;
    if (humans(g) < 2) return title;
    k = (k + 1) % 4;
    snprintf(buf[k], sizeof buf[k], "%s: %s", nm(g, p), title);
    return buf[k];
}
#define TT(title) for_player(g, p, title)

/* Ask with the space outlined on the board. */
static int ask_at(int sp, const char *title, const char *text, const char *const *b, int n, int def, int esc)
{
    int save = app.highlight;
    app.highlight = sp;
    int r = ui_ask(title, text, b, n, def, esc);
    app.highlight = save;
    return r;
}

/* ---- Rule 1 ---- */

static const int *deal_cards;
static int deal_n, deal_cash;

/* The footer of the deal's check list: the total and what would be left. */
static void deal_footer(const int *checked, char *out, size_t size)
{
    int total = 0;
    for (int i = 0; i < deal_n; i++) if (checked[i]) total += board[deal_cards[i]].price;
    snprintf(out, size, "Total $%d; you would keep $%d.", total, deal_cash - total);
}

/* Rule 1: the cards dealt to p, as a check list; refuses a choice he cannot
 * pay for. */
static void deal_buy(Game *g, int p, const int *cards, int n, int *want)
{
    if (n <= 0) return;
    char items[NSPACES][96];
    const char *ptr[NSPACES];
    for (int i = 0; i < n; i++) {
        int sp = cards[i];
        if (board[sp].type == T_LOT)
            snprintf(items[i], sizeof items[i], "%s  -  $%d  (land rent $%d)", board[sp].name, board[sp].price, board[sp].rent);
        else if (board[sp].type == T_SPECULATION)
            snprintf(items[i], sizeof items[i], "%s (Broker's License)  -  $%d", board[sp].name, board[sp].price);
        else
            snprintf(items[i], sizeof items[i], "%s  -  $%d  (charter)", board[sp].name, board[sp].price);
        ptr[i] = items[i];
    }
    deal_cards = cards;
    deal_n = n;
    deal_cash = g->pl[p].cash;
    char note[400];
    snprintf(note, sizeof note,
             "These cards have been dealt to you. Tick the ones you want to buy (Space or a click), "
             "then OK. You have $%d. Every card counts 100 at the end of the game; the rules advise keeping "
             "$50 to $75 for expenses around the board.", g->pl[p].cash);
    for (;;) {
        if (!ui_checklist(TT("The Deal (Rule 1)"), note, ptr, n, want, deal_footer)) {
            memset(want, 0, sizeof *want * (size_t)n);
            return;
        }
        int total = 0;
        for (int i = 0; i < n; i++) if (want[i]) total += board[cards[i]].price;
        if (total <= g->pl[p].cash) return;
        ui_message(TT("The Deal"), "You have not money enough for all of those cards.");
    }
}

/* ---- buying and bidding ---- */

static void describe_card(const Game *g, int sp, char *out, size_t size)
{
    const Space *s = &board[sp];
    switch (s->type) {
    case T_LOT:
        snprintf(out, size, "land rent $%d, and $10 for each house", game_land_rent(g, sp));
        break;
    case T_RAILROAD:
        snprintf(out, size, "a fare of $5; $10, $20 or $50 with 2, 3 or 4 railroads");
        break;
    case T_FRANCHISE:
        snprintf(out, size, "a charge of $5; $25 with both franchises (the Municipal Cinch)");
        break;
    case T_SPECULATION:
        snprintf(out, size, "the broker takes the $10 ante and a tenth of every speculator's winnings");
        break;
    default:
        out[0] = '\0';
    }
}

/* Rule 7: buy sp at `price` (the sale price, or the highest bid when
 * another player bid more)? */
static int h_buy(Game *g, int p, int sp, int price, int high_bidder)
{
    char text[600], what[160];
    describe_card(g, sp, what, sizeof what);
    if (high_bidder >= 0)
        snprintf(text, sizeof text,
                 "%s is for sale. %s has bid $%d for it, and you have the first option at that price.\n\n"
                 "It brings %s. You have $%d.", board[sp].name, nm(g, high_bidder), price, what, g->pl[p].cash);
    else
        snprintf(text, sizeof text,
                 "%s is for sale at $%d.\n\nIt brings %s. You have $%d. If you do not buy it, %s.",
                 board[sp].name, price, what, g->pl[p].cash,
                 board[sp].type == T_SPECULATION ? "nobody holds the license"
                                                 : "you pay the charge into the Public Treasury");
    static const char *const b[] = { "&Buy", "&Pass" };
    return ask_at(sp, TT("For Sale (Rule 7)"), text, b, 2, 0, 1) == 0;
}

/* Rule 7: another player stopped on unowned sp; how much will p bid (0 =
 * none)? Not asked when Options > Ask Me to Bid is off. */
static int h_max_bid(Game *g, int p, int sp, int min)
{
    if (!app.prefs.ask_bids) return 0;
    char note[500], what[160];
    describe_card(g, sp, what, sizeof what);
    snprintf(note, sizeof note,
             "%s has stopped on %s (sale price $%d), which brings %s. You may bid more than the price "
             "(at least $%d); %s has the first option at the highest bid. You have $%d.\n"
             "Enter the most you will pay, or 0 for no bid.",
             nm(g, g->cur), board[sp].name, board[sp].price, what, min, nm(g, g->cur), g->pl[p].cash);
    int v = 0;
    int save = app.highlight;
    app.highlight = sp;
    int ok = ui_number(TT("Bidding (Rule 7)"), note, &v);
    app.highlight = save;
    return ok && v >= min ? v : 0;
}

/* ---- choices on the way ---- */

static int h_jail_pay(Game *g, int p)
{
    char text[300];
    snprintf(text, sizeof text,
             "You are in Jail (turn %d of at most 3). Pay the $50 fine and throw, or try to throw a double "
             "(\"serving your time\")? You have $%d.", g->pl[p].jail_turns, g->pl[p].cash);
    static const char *const b[] = { "&Pay $50", "&Throw for a Double" };
    return ui_ask(TT("Jail (Rule 6)"), text, b, 2, 1, -1) == 0;
}

/* Rule 17: use the railroad pass a double gives? */
static int h_use_pass(Game *g, int p, int with, int without)
{
    (void)p;
    char text[400];
    snprintf(text, sizeof text,
             "You threw a double: that is an official railroad pass. Jump the nine spaces between the next two "
             "corners and stop on %s, or move straight ahead to %s?",
             board[with].name, board[without].name);
    char b1[80], b2[80];
    snprintf(b1, sizeof b1, "&Use the Pass");
    snprintf(b2, sizeof b2, "&Move Straight");
    const char *const b[] = { b1, b2 };
    (void)g;
    return ask_at(with, TT("Railroad Pass (Rule 17)"), text, b, 2, 0, -1) == 0;
}

/* Rule 2: forward or backward between the Chances? */
static int h_direction(Game *g, int p, int fwd, int back)
{
    (void)g; (void)p;
    char text[300];
    snprintf(text, sizeof text,
             "Between the Chances you may move either way. Forward to %s, or backward to %s?",
             board[fwd].name, board[back].name);
    static const char *const b[] = { "&Forward", "&Backward" };
    return ask_at(fwd, TT("Which Way? (Rule 2)"), text, b, 2, 0, -1) == 0;
}

/* Rule 8: play the broker's game, or refuse to move? */
static int h_speculate(Game *g, int p)
{
    char text[400];
    int broker = g->owner[21];
    snprintf(text, sizeof text,
             "Your throw would bring you to SPECULATION. Play the broker's game - a $10 ante%s, then throw "
             "again: a double wins $100, 11 wins $90 and so on down to 5 for $30, but 3 or 4 is a \"skin game\" "
             "and wins nothing - or refuse to move and stay where you are?",
             broker != NOBODY && broker != p ? " to the broker" : "");
    static const char *const b[] = { "&Speculate", "Stay &Put" };
    return ask_at(21, TT("Speculation (Rule 8)"), text, b, 2, 0, -1) == 0;
}

/* Rule 13: buy the luxury, or move backward? */
static int h_luxury(Game *g, int p)
{
    char text[300];
    snprintf(text, sizeof text,
             "LUXURY: pay $75 for a luxury card, which counts 100 at the end of the game? If you do without it "
             "you move backward instead, as far as you threw. You have $%d.", g->pl[p].cash);
    static const char *const b[] = { "&Buy the Luxury", "Move &Backward" };
    return ask_at(g->pl[p].pos, TT("Luxury (Rule 13)"), text, b, 2, 0, -1) == 0;
}

/* Single Tax Rule 4: build on the unimproved lot just landed on? */
static int h_st_build(Game *g, int p, int sp)
{
    char text[300];
    snprintf(text, sizeof text,
             "%s is unimproved. Under the Single Tax anyone may build there: put up a house for $100? You "
             "will collect $10 house rent from every other player who stops here. You have $%d.",
             board[sp].name, g->pl[p].cash);
    static const char *const b[] = { "&Build", "&No" };
    return ask_at(sp, TT("Build (Single Tax Rule 4)"), text, b, 2, 0, 1) == 0;
}

/* ---- raising money ---- */

typedef struct { int kind, sp, buyer, price; } Sale;   /* kind: 1 house, 2 to a player, 3 to the pack */

int human_choose_sale(Game *g, int p, const char *title, const char *note, int need,
                      int *kind, int *sp_out, int *buyer, int *price)
{
    title = for_player(g, p, title);
    Sale sales[3 * NSPACES];
    char items[3 * NSPACES][120];
    const char *ptr[3 * NSPACES];
    int n = 0;
    for (int sp = 0; sp < NSPACES; sp++) {
        if (g->house_owner[sp] == p && g->houses[sp]) {
            sales[n] = (Sale){ 1, sp, -1, HOUSE_RESALE };
            snprintf(items[n], sizeof items[n], "Sell a house on %s  -  $%d", board[sp].name, HOUSE_RESALE);
            n++;
        }
        if (g->owner[sp] != p || (g->single_tax && board[sp].type == T_LOT)) continue;
        if (g->houses[sp] && !g->single_tax) continue;
        int best = -1, best_price = 0;
        for (int q = 0; q < g->nplayers; q++) {
            if (q == p || !g->pl[q].cpu || !game_active(g, q)) continue;
            int o = ai_offer_for(g, q, sp);
            if (o > best_price) { best_price = o; best = q; }
        }
        if (best >= 0 && best_price > PACK_PRICE(sp)) {
            sales[n] = (Sale){ 2, sp, best, best_price };
            snprintf(items[n], sizeof items[n], "Sell %s to %s  -  about $%d", board[sp].name, nm(g, best), best_price);
            n++;
        }
        sales[n] = (Sale){ 3, sp, -1, PACK_PRICE(sp) };
        snprintf(items[n], sizeof items[n], "Sell %s back to the pack  -  $%d", board[sp].name, PACK_PRICE(sp));
        n++;
    }
    if (!n) {
        if (need > 0) ui_message(title, "You have nothing left to sell.");
        return 0;
    }
    for (int i = 0; i < n; i++) ptr[i] = items[i];
    int i = ui_choose(title, note, ptr, n, 0);
    if (i < 0) return 0;
    *kind = sales[i].kind;
    *sp_out = sales[i].sp;
    *buyer = sales[i].buyer;
    *price = sales[i].price;
    return 1;
}

/* See human.h: choose a sale and carry it out at once. */
int human_sell_menu(Game *g, int p, const char *title, const char *note, int need)
{
    int kind, sp, buyer, price;
    if (!human_choose_sale(g, p, title, note, need, &kind, &sp, &buyer, &price)) return 0;
    const char *err = kind == 1 ? game_sell_house(g, p, sp)
                    : kind == 2 ? game_trade(g, p, buyer, sp, price)
                    : game_sell_to_pack(g, p, sp);
    if (err) { ui_message(for_player(g, p, title), err); return 0; }
    return 1;
}

/* See human.h: what to sell to find `need` dollars, asking again before
 * giving up. */
int human_raise_choice(Game *g, int p, int need, int *kind, int *sp, int *buyer)
{
    char note[300];
    int price;
    snprintf(note, sizeof note,
             "You must find $%d more (you have $%d). Choose something to sell. Cancel gives up: you pay what "
             "you have and go to the %s.", need, g->pl[p].cash, g->single_tax ? "Natural Opportunity" : "Poor House");
    for (;;) {
        if (human_choose_sale(g, p, "Raising Money", note, need, kind, sp, buyer, &price)) return 1;
        if (ui_yes_no(TT("Raising Money"), g->single_tax
                          ? "Give up and rest on a Natural Opportunity?"
                          : "Give up and go to the Poor House?", 0))
            return 0;
    }
}

/* The Decider's raise: choose a sale and carry it out, or give up (0). */
static int h_raise(Game *g, int p, int need)
{
    char note[300];
    snprintf(note, sizeof note,
             "You must find $%d more (you have $%d). Choose something to sell. Cancel gives up: you pay what "
             "you have and go to the %s.", need, g->pl[p].cash, g->single_tax ? "Natural Opportunity" : "Poor House");
    for (;;) {
        if (human_sell_menu(g, p, "Raising Money", note, need)) return 1;
        if (ui_yes_no(TT("Raising Money"), g->single_tax
                          ? "Give up and rest on a Natural Opportunity?"
                          : "Give up and go to the Poor House?", 0))
            return 0;
    }
}

/* ---- votes ---- */

static int h_vote(Game *g, int p, int proposer)
{
    char text[500];
    int lots = game_cards_of(g, p, T_LOT);
    snprintf(text, sizeof text,
             "%s proposes the SINGLE TAX. If it passes, all land rent goes into the Public Treasury, title "
             "deeds to lots are worth nothing (you hold %d), there are no taxes on necessities, the treasury buys "
             "the utilities for the public, builds free colleges and raises wages.\n\nDo you vote for it?",
             nm(g, proposer), lots);
    return ui_yes_no(TT("The Single Tax"), text, 0);
}

/* Rule 9: join the other owner of the railroads in a Trust? */
static int h_join_trust(Game *g, int p, int other)
{
    (void)p;
    char text[300];
    snprintf(text, sizeof text,
             "You and %s own all the railroads between you. Pool them in a TRUST? Every railroad fare becomes "
             "$40, and the two of you divide the profits.", nm(g, other));
    return ui_yes_no(TT("The Trust (Rule 9)"), text, 1);
}

/* ---- trades between humans ---- */

int human_accepts_sale(Game *g, int seller, int buyer, int sp, int price)
{
    char text[400];
    int p = seller;
    char pass[64] = "";
    if (humans(g) > 1) snprintf(pass, sizeof pass, "Pass the game to %s.\n\n", nm(g, seller));
    snprintf(text, sizeof text, "%s%s offers you $%d for %s (sale price $%d). Sell it?",
             pass, nm(g, buyer), price, board[sp].name, board[sp].price);
    static const char *const b[] = { "&Sell", "&Keep It" };
    return ask_at(sp, TT("An Offer"), text, b, 2, 1, 1) == 0;
}

/* See human.h: the buyer is asked whether he will pay the seller's price. */
int human_accepts_purchase(Game *g, int buyer, int seller, int sp, int price)
{
    char text[400];
    int p = buyer;
    char pass[64] = "";
    if (humans(g) > 1) snprintf(pass, sizeof pass, "Pass the game to %s.\n\n", nm(g, buyer));
    snprintf(text, sizeof text, "%s%s offers to sell you %s for $%d (sale price $%d). You have $%d. Buy it?",
             pass, nm(g, seller), board[sp].name, price, board[sp].price, g->pl[buyer].cash);
    static const char *const b[] = { "&Buy", "&No Thanks" };
    return ask_at(sp, TT("An Offer"), text, b, 2, 1, 1) == 0;
}

const Decider human_decider = {
    .deal_buy = deal_buy,
    .buy = h_buy,
    .max_bid = h_max_bid,
    .jail_pay = h_jail_pay,
    .use_pass = h_use_pass,
    .direction = h_direction,
    .speculate = h_speculate,
    .luxury = h_luxury,
    .st_build = h_st_build,
    .raise = h_raise,
    .vote_single_tax = h_vote,
    .join_trust = h_join_trust,
    .pre_roll = NULL,
};
