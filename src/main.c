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

/* main.c - Viola the Landlord: start-up, the main loop and the commands.
 *
 * THE MAIN LOOP
 *   While a game is on: if it is a computer player's turn, play it (the
 *   game's hooks redraw the window and pause so the moves can be watched);
 *   if it is the human's turn, wait for a key, a click or a menu command.
 *   The Roll command plays the human's turn; dialogs ask his choices. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ai.h"
#include "app.h"
#include "help.h"
#include "human.h"
#include "keys.h"
#include "menubar.h"
#include "net.h"
#include "netui.h"
#include "ui.h"
#include "util.h"
#include "version.h"
#include "view.h"
#include "xwin.h"

App app;

static int humans_in_game(const Game *g);
static int remote_seat(int p);
static int quit_requested;
static int final_shown;

/* ---- for the menus ---- */

int app_can_act(void) { return app.playing && app.human_turn && !app.game.over; }
int app_can_propose(void) { return app_can_act() && !app.game.single_tax; }
int app_bids_on(void) { return app.prefs.ask_bids; }
int app_confirm_on(void) { return app.prefs.confirm_cpu; }

static void redraw(void) { view_draw(); }

/* ---- the game's hooks ---- */

/* Wait `ms`, keeping the window alive; a key or click cuts it short. */
static void watch(int ms)
{
    int k = xw_wait_key(ms);
    if (k == K_CLOSE) quit_requested = 1;
    if (k == K_RESIZE) { view_draw(); xw_flush(); }
}

static int cpu_turn(const Game *g) { return app.playing && !g->over && g->pl[g->cur].cpu; }

static void hook_changed(Game *g)
{
    if (net_role() == NET_HOST) net_broadcast(g);
    view_scroll_log(0);
    view_draw();
    xw_flush();
    if (cpu_turn(g) && app.cpu_running && !quit_requested) watch(prefs_delay(&app.prefs) / 2);
}

static void hook_dice(Game *g, int d1, int d2)
{
    (void)d1; (void)d2;
    if (app.prefs.speed == 0 && g->pl[g->cur].cpu) return;
    for (int i = 1; i <= 6; i++) {
        app.dice_rolling = i + g->turn_no;
        view_draw();
        xw_flush();
        xw_wait_key(45);
    }
    app.dice_rolling = 0;
    view_draw();
    xw_flush();
}

static void hook_chance(Game *g, int p, const char *text)
{
    if (g->pl[p].cpu) return;
    if (remote_seat(p)) { net_note(p, "Chance", text); return; }
    char title[64];
    if (humans_in_game(g) > 1) snprintf(title, sizeof title, "%s: Chance", g->pl[p].name);
    else snprintf(title, sizeof title, "Chance");
    ui_message(title, text);
}

static void hook_pause(Game *g, int p)
{
    if (p < 0 || !g->pl[p].cpu || quit_requested) return;
    view_draw();
    xw_flush();
    if (app.prefs.confirm_cpu) {
        view_set_status("%s has moved. Press a key to go on.", g->pl[p].name);
        view_draw();
        xw_flush();
        int k;
        do k = xw_next_key(); while (k == K_RESIZE ? (view_draw(), xw_flush(), 1) : 0);
        if (k == K_CLOSE) quit_requested = 1;
    } else {
        watch(prefs_delay(&app.prefs));
    }
}

/* ---- starting a game ---- */

static unsigned long time_seed(void)
{
    return (unsigned long)time(NULL) * 2654435761UL ^ (unsigned long)getpid();
}

static const char *const cpu_names[] = { "Lord Blueblood", "Mr. D. F. Hogg", "Mrs. Soakum", "Mr. Slambang" };

/* Humans at this screen: not computer players, not network joiners. */
static int humans_in_game(const Game *g)
{
    if (net_role() == NET_CLIENT) return 1;
    int n = 0;
    for (int p = 0; p < g->nplayers; p++) n += !g->pl[p].cpu && !net_seat_remote(p);
    return n;
}

int app_local_humans(void) { return app.playing ? humans_in_game(&app.game) : 1; }

/* Is player p played at another machine (a joiner still connected)? */
static int remote_seat(int p) { return net_role() == NET_HOST && net_seat_connected(p); }

/* A message for player p, wherever he sits. */
static void notify(int p, const char *title, const char *text)
{
    if (remote_seat(p)) net_note(p, title, text);
    else if (!app.game.pl[p].cpu) ui_message(title, text);
}

/* A joiner's command goes to the host as an ACTION. */
static int joiner(void) { return net_role() == NET_CLIENT; }

static void send_action(int kind, int a, int b, int c)
{
    int n[4] = { kind, a, b, c };
    if (net_client_send(NM_ACTION, n, 4, NULL) < 0) ui_message("Network Game", "The connection to the host is closed.");
}

/* The human whose turn was last, for the "Pass to ..." prompt. */
static int last_human = -1;

static void set_hooks(Game *g)
{
    g->hooks.changed = hook_changed;
    g->hooks.dice = hook_dice;
    g->hooks.chance = hook_chance;
    g->hooks.pause = hook_pause;
}

/* Log who is playing and deal the cards. */
static void begin(Game *g)
{
    app.playing = 1;
    app.human_turn = 0;
    app.highlight = -1;
    final_shown = 0;
    last_human = -1;
    char who[240] = "";
    for (int p = 0; p < g->nplayers; p++) {
        char one[64];
        static const char *const dn[] = { "easy", "medium", "hard" };
        if (g->pl[p].cpu) snprintf(one, sizeof one, "%s (%s)", g->pl[p].name, dn[CLAMP(g->pl[p].difficulty, 0, 2)]);
        else if (net_seat_remote(p)) snprintf(one, sizeof one, "%s (network)", g->pl[p].name);
        else snprintf(one, sizeof one, "%s", g->pl[p].name);
        if (p) strncat(who, p == g->nplayers - 1 ? " and " : ", ", sizeof who - strlen(who) - 1);
        strncat(who, one, sizeof who - strlen(who) - 1);
    }
    game_log(g, "A new game: %s; the game ends at %d wages%s.", who, g->end_wages,
             g->advanced ? ", advanced rules" : "");
    view_set_status("The deal.");
    app.cpu_running = 1;
    game_deal(g);
    app.cpu_running = 0;
}

static void start_game(void)
{
    GameSettings s;
    memset(&s, 0, sizeof s);
    s.advanced = app.prefs.advanced;
    s.end_wages = app.prefs.end_wages;
    s.seed = (unsigned long)time_seed();
    /* Empty seats are skipped; the others keep their order. */
    int ncpu = 0, kinds[MAXPLAYERS];
    for (int i = 0; i < NSEATS && s.nplayers < MAXPLAYERS; i++) {
        int kind = app.prefs.seat[i];
        if (kind == SEAT_EMPTY || kind == SEAT_NETWORK) continue;   /* (network seats: see net.h) */
        int p = s.nplayers++;
        kinds[p] = kind;
        if (kind == SEAT_HUMAN) {
            copy_str(s.names[p], NAME_LEN, app.prefs.names[i]);
        } else {
            s.cpu[p] = 1;
            s.difficulty[p] = prefs_seat_difficulty(kind);
            copy_str(s.names[p], NAME_LEN, cpu_names[ncpu++ % 4]);
        }
    }
    game_new(&app.game, &s);
    Game *g = &app.game;
    set_hooks(g);
    for (int p = 0; p < g->nplayers; p++)
        game_set_decider(g, p, kinds[p] == SEAT_HUMAN ? &human_decider : &ai_decider);
    begin(g);
}

/* Hosting: the setup form and the lobby (netui.c), then the game, with
 * the joiners' seats played through net_decider. */
static int host_action(Game *g, int p, const NetMsg *m);

static void start_network_game(void)
{
    GameSettings s;
    if (!netui_host_setup(&s)) return;
    s.seed = time_seed();
    game_new(&app.game, &s);
    Game *g = &app.game;
    set_hooks(g);
    for (int p = 0; p < g->nplayers; p++)
        game_set_decider(g, p, p == 0 ? &human_decider : g->pl[p].cpu ? &ai_decider : &net_decider);
    net_set_action_handler(host_action);
    net_start_game(g);
    begin(g);
}

/* The settings form: who sits where, the length and the rules. Returns 1 if
 * confirmed (and the seats make a table). */
static int settings_form(const char *title, const char *ok)
{
    static const char *const kinds[] = { "Empty", "Human", "Easy", "Medium", "Hard" };
    static const char *const lens[] = { "3", "5", "7", "10" };
    static const char *const rules[] = { "Standard", "Advanced" };
    static const int len_val[] = { 3, 5, 7, 10 };
    UiRow rows[] = {
        { "Seat 1", kinds, 5 }, { "Seat 2", kinds, 5 }, { "Seat 3", kinds, 5 }, { "Seat 4", kinds, 5 },
        { "Game ends at wages", lens, 4 },
        { "Rules", rules, 2 },
    };
    int sel[6];
    for (int i = 0; i < NSEATS; i++) sel[i] = app.prefs.seat[i] == SEAT_NETWORK ? SEAT_EMPTY : app.prefs.seat[i];
    sel[4] = 1;
    for (int i = 0; i < 4; i++) if (len_val[i] == app.prefs.end_wages) sel[4] = i;
    sel[5] = app.prefs.advanced;
    for (;;) {
        if (!ui_form(title,
                     "Seats play in order. Human seats share this screen and take turns at it; Easy, Medium and "
                     "Hard are computer players. The 1906 rules end the game when a player has drawn his wages "
                     "five times. The advanced rules double taxes and land rents as houses go up.",
                     rows, 6, sel, ok))
            return 0;
        int used = 0, hum = 0;
        for (int i = 0; i < NSEATS; i++) { used += sel[i] != SEAT_EMPTY; hum += sel[i] == SEAT_HUMAN; }
        if (used >= 2 && hum >= 1) break;
        ui_message(title, used < 2 ? "The Landlord's Game needs at least two players."
                                   : "At least one seat must be a human player.");
    }
    for (int i = 0; i < NSEATS; i++) app.prefs.seat[i] = sel[i];
    app.prefs.end_wages = len_val[sel[4]];
    app.prefs.advanced = sel[5];
    /* Names for the human seats (Enter keeps the one shown). */
    for (int i = 0; i < NSEATS; i++) {
        if (app.prefs.seat[i] != SEAT_HUMAN) continue;
        char name[32], note[80];
        copy_str(name, sizeof name, app.prefs.names[i]);
        snprintf(note, sizeof note, "The name of the human player in seat %d:", i + 1);
        if (ui_prompt("Player's Name", note, name, sizeof name) && name[0])
            copy_str(app.prefs.names[i], sizeof app.prefs.names[i], name);
    }
    prefs_save(&app.prefs);
    return 1;
}

static void cmd_new_game(void)
{
    if (app.playing && !app.game.over &&
        !ui_yes_no("New Game", "Abandon the game in progress?", 0))
        return;
    if (!settings_form("New Game", "&Start")) return;
    if (net_role() != NET_NONE) net_close("The game has been closed.");
    app.me = 0;
    start_game();
}

static void cmd_settings(void)
{
    if (!settings_form("Game Settings", "OK")) return;
    if (app.playing) ui_message("Game Settings", "The new settings take effect in the next game.");
}

static void cmd_speed(void)
{
    static const char *const items[] = { "Fast", "Normal", "Slow" };
    int i = ui_choose("Computer Speed", "How long to pause after each computer move:", items, 3, app.prefs.speed);
    if (i >= 0) { app.prefs.speed = i; prefs_save(&app.prefs); }
}

/* ---- the business of the human whose turn it is ----
 *
 * The cmd_* functions ask the player at this screen what he wants; the act_*
 * functions do it. A network joiner's act_* send an ACTION to the host,
 * whose host_action() calls the same act_* for the joiner's seat, so a
 * build, a trade or a proposal works the same wherever it comes from. */

/* The player acting at this screen: the current player, or a joiner's own seat. */
static int me(void) { return joiner() ? app.me : app.game.cur; }

/* Would `seller` sell card sp to `buyer` for `price`? Asks him wherever he sits. */
static int seat_accepts_sale(int seller, int buyer, int sp, int price)
{
    Game *g = &app.game;
    if (g->pl[seller].cpu) return ai_accepts_sale(g, seller, sp, price);
    if (remote_seat(seller)) {
        int r = net_ask(seller, NA_ACCEPT_SALE, buyer, sp, price, NULL);
        return r >= 0 ? r : ai_accepts_sale(g, seller, sp, price);
    }
    return human_accepts_sale(g, seller, buyer, sp, price);
}

/* Would `buyer` pay `price` for `seller`'s card sp? */
static int seat_accepts_purchase(int buyer, int seller, int sp, int price)
{
    Game *g = &app.game;
    if (g->pl[buyer].cpu) return ai_offer_for(g, buyer, sp) >= price;
    if (remote_seat(buyer)) {
        int r = net_ask(buyer, NA_ACCEPT_PURCHASE, seller, sp, price, NULL);
        return r > 0;
    }
    return human_accepts_purchase(g, buyer, seller, sp, price);
}

static void act_build(int p, int sp)
{
    if (joiner()) { send_action(NX_BUILD, sp, 0, 0); return; }
    const char *err = game_build(&app.game, p, sp);
    if (err) notify(p, "Build a House", err);
}

static void act_trade_buy(int buyer, int seller, int sp, int price)
{
    if (joiner()) { send_action(NX_TRADE_BUY, seller, sp, price); return; }
    Game *g = &app.game;
    if (seller < 0 || seller >= g->nplayers || g->owner[sp] != seller || price <= 0 || price > g->pl[buyer].cash) {
        notify(buyer, "Buy a Card", "That offer cannot be made.");
        return;
    }
    int yes = seat_accepts_sale(seller, buyer, sp, price);
    char msg[200];
    if (yes && !game_trade(g, seller, buyer, sp, price)) {
        snprintf(msg, sizeof msg, "%s accepts: %s goes to %s for $%d.", g->pl[seller].name, board[sp].name,
                 g->pl[buyer].name, price);
        notify(buyer, "Sold", msg);
    } else {
        snprintf(msg, sizeof msg, "%s declines the offer of $%d for %s.", g->pl[seller].name, price, board[sp].name);
        notify(buyer, "Declined", msg);
    }
    if (humans_in_game(g) > 1 && !g->pl[seller].cpu && !remote_seat(seller)) last_human = seller;
}

static void act_trade_sell(int seller, int buyer, int sp, int price)
{
    if (joiner()) { send_action(NX_TRADE_SELL, buyer, sp, price); return; }
    Game *g = &app.game;
    if (buyer < 0 || buyer >= g->nplayers || buyer == seller || g->owner[sp] != seller || price <= 0) {
        notify(seller, "Sell a Card", "That sale cannot be made.");
        return;
    }
    if (price > g->pl[buyer].cash) { notify(seller, "Sell a Card", "The buyer has not that much money."); return; }
    int yes = seat_accepts_purchase(buyer, seller, sp, price);
    char msg[200];
    if (yes && !game_trade(g, seller, buyer, sp, price)) {
        snprintf(msg, sizeof msg, "%s buys %s for $%d.", g->pl[buyer].name, board[sp].name, price);
        notify(seller, "Sold", msg);
    } else {
        snprintf(msg, sizeof msg, "%s will not pay $%d for %s.", g->pl[buyer].name, price, board[sp].name);
        notify(seller, "Declined", msg);
    }
    if (humans_in_game(g) > 1 && !g->pl[buyer].cpu && !remote_seat(buyer)) last_human = buyer;
}

/* A sale to the pack, of a house, or to a computer player at his offer. */
static void act_sell(int p, int kind, int sp, int buyer)
{
    if (joiner()) { send_action(NX_SELL, net_sale_encode(kind, sp, buyer), 0, 0); return; }
    Game *g = &app.game;
    if (sp < 0 || sp >= NSPACES) return;
    const char *err = "That cannot be sold.";
    if (kind == 1) err = game_sell_house(g, p, sp);
    else if (kind == 3) err = game_sell_to_pack(g, p, sp);
    else if (kind == 2 && buyer >= 0 && buyer < g->nplayers && g->pl[buyer].cpu) {
        int offer = ai_offer_for(g, buyer, sp);
        err = offer > 0 ? game_trade(g, p, buyer, sp, offer) : "No offer is made for that card now.";
    }
    if (err) notify(p, "Sell", err);
}

static void act_propose(int p)
{
    if (joiner()) { send_action(NX_PROPOSE, 0, 0, 0); return; }
    if (app.game.single_tax) return;
    app.cpu_running = 0;
    int ok = game_propose_single_tax(&app.game, p);
    notify(p, "The Single Tax", ok ? "The Single Tax is in force!" : "The Single Tax is defeated.");
    last_human = -2;                            /* other humans voted: say whose turn it is again */
}

/* A joiner's ACTION, at the host (net.c calls it during the joiner's turn). */
static int host_action(Game *g, int p, const NetMsg *m)
{
    int a = m->num[1], b = m->num[2], c = m->num[3];
    int sp_ok = b >= 0 && b < NSPACES;
    switch (m->num[0]) {
    case NX_BUILD:      if (a >= 0 && a < NSPACES) act_build(p, a); break;
    case NX_TRADE_BUY:  if (sp_ok) act_trade_buy(p, a, b, c); break;
    case NX_TRADE_SELL: if (sp_ok) act_trade_sell(p, a, b, c); break;
    case NX_SELL: {
        int kind, sp, buyer;
        net_sale_decode(a, &kind, &sp, &buyer);
        act_sell(p, kind, sp, buyer);
        break;
    }
    case NX_PROPOSE:    act_propose(p); break;
    default: break;
    }
    view_draw();
    xw_flush();
    (void)g;
    return 0;
}

static void cmd_build(void)
{
    Game *g = &app.game;
    int p = me();
    for (;;) {
        int sps[NSPACES], n = 0;
        char items[NSPACES][120];
        const char *ptr[NSPACES];
        for (int sp = 0; sp < NSPACES; sp++) {
            if (!game_can_build(g, p, sp)) continue;
            snprintf(items[n], sizeof items[n], "%s  -  %d house%s now, rent $%d + $%d", board[sp].name,
                     g->houses[sp], g->houses[sp] == 1 ? "" : "s", game_land_rent(g, sp), 10 * g->houses[sp]);
            ptr[n] = items[n];
            sps[n++] = sp;
        }
        if (!n) {
            ui_message("Build a House", g->single_tax ? "There is no lot you can build on."
                                                      : "You own no lot to build on. Buy lots first.");
            return;
        }
        char note[200];
        snprintf(note, sizeof note, "A house costs $100, counts 100 at the end, and adds $10 to the rent. "
                 "%s has $%d.", g->pl[p].name, g->pl[p].cash);
        int i = ui_choose("Build a House (Rule 14)", note, ptr, n, 0);
        if (i < 0) return;
        if (g->pl[p].cash < HOUSE_PRICE) { ui_message("Build a House", "A house costs $100."); return; }
        act_build(p, sps[i]);
        if (joiner()) { netui_client_pump(500); }   /* see the house before choosing again */
    }
}

/* Can this card change hands between players? */
static int tradable(const Game *g, int sp)
{
    return !(g->single_tax && board[sp].type == T_LOT) && !(g->houses[sp] && !g->single_tax);
}

static void cmd_trade_buy(void)
{
    Game *g = &app.game;
    int p = me();
    int sps[NSPACES], n = 0;
    char items[NSPACES][120];
    const char *ptr[NSPACES];
    for (int sp = 0; sp < NSPACES; sp++) {
        int o = g->owner[sp];
        if (o == NOBODY || o == p || !tradable(g, sp)) continue;
        snprintf(items[n], sizeof items[n], "%s  (%s)", board[sp].name, g->pl[o].name);
        ptr[n] = items[n];
        sps[n++] = sp;
    }
    if (!n) { ui_message("Buy a Card", "The other players hold no card they could sell you."); return; }
    int i = ui_choose("Buy a Card from a Player", "Which card would you like to buy?", ptr, n, 0);
    if (i < 0) return;
    int sp = sps[i], o = g->owner[sp];
    int offer = board[sp].price;
    char note[300];
    snprintf(note, sizeof note, "How much do you offer %s for %s? (Sale price $%d; you have $%d.)",
             g->pl[o].name, board[sp].name, board[sp].price, g->pl[p].cash);
    if (!ui_number("Your Offer", note, &offer) || offer <= 0) return;
    if (offer > g->pl[p].cash) { ui_message("Your Offer", "You have not that much money."); return; }
    act_trade_buy(p, o, sp, offer);
}

static void cmd_trade_sell(void)
{
    Game *g = &app.game;
    int p = me();
    int sps[NSPACES], n = 0;
    char items[NSPACES][120];
    const char *ptr[NSPACES];
    for (int sp = 0; sp < NSPACES; sp++) {
        if (g->owner[sp] != p || !tradable(g, sp)) continue;
        snprintf(items[n], sizeof items[n], "%s  (sale price $%d)", board[sp].name, board[sp].price);
        ptr[n] = items[n];
        sps[n++] = sp;
    }
    if (!n) { ui_message("Sell a Card", "You hold no card you can sell (lots with houses must be cleared first)."); return; }
    int i = ui_choose("Sell a Card to a Player", "Which card would you like to sell?", ptr, n, 0);
    if (i < 0) return;
    int sp = sps[i];

    /* The buyers: each computer player's own offer, or a human (here or
     * over the network) named with an asking price. */
    char bitems[MAXPLAYERS][120];
    const char *bptr[MAXPLAYERS];
    int who[MAXPLAYERS], price[MAXPLAYERS], nb = 0;
    for (int q = 0; q < g->nplayers; q++) {
        if (q == p) continue;
        if (g->pl[q].cpu) {
            int o = ai_offer_for(g, q, sp);
            if (o <= 0) continue;
            snprintf(bitems[nb], sizeof bitems[nb], "%s offers $%d", g->pl[q].name, o);
            price[nb] = o;
        } else {
            snprintf(bitems[nb], sizeof bitems[nb], "Offer it to %s at a price you name...", g->pl[q].name);
            price[nb] = -1;
        }
        bptr[nb] = bitems[nb];
        who[nb++] = q;
    }
    if (!nb) { ui_message("No Offers", "Nobody will make you an offer for that card just now."); return; }
    char note[160];
    snprintf(note, sizeof note, "Who should have %s?", board[sp].name);
    int k = ui_choose("Sell a Card to a Player", note, bptr, nb, 0);
    if (k < 0) return;
    int q = who[k];
    int ask = price[k];
    if (ask < 0) {
        ask = board[sp].price;
        snprintf(note, sizeof note, "Your price to %s for %s? (Sale price $%d; %s has $%d.)", g->pl[q].name,
                 board[sp].name, board[sp].price, g->pl[q].name, g->pl[q].cash);
        if (!ui_number("Your Price", note, &ask) || ask <= 0) return;
        if (ask > g->pl[q].cash) { ui_message("Your Price", "The buyer has not that much money."); return; }
    }
    act_trade_sell(p, q, sp, ask);
}

static void cmd_sell_pack(void)
{
    Game *g = &app.game;
    int p = me();
    char note[200];
    snprintf(note, sizeof note, "Cards go back to the pack for half their sale price, houses to the bank for "
             "$50, or to a computer player at his offer. You have $%d.", g->pl[p].cash);
    int kind, sp, buyer, price;
    while (human_choose_sale(g, p, "Sell", note, 0, &kind, &sp, &buyer, &price)) {
        act_sell(p, kind, sp, buyer);
        if (joiner()) netui_client_pump(500);
        snprintf(note, sizeof note, "Sell anything else? You have $%d.", g->pl[p].cash);
    }
}

static void cmd_single_tax(void)
{
    if (!ui_yes_no("The Single Tax",
                   "Propose the Single Tax? It needs one more vote than yours. Under it land rent goes to the "
                   "Public Treasury and title deeds to lots are worth nothing; the treasury frees the utilities, "
                   "builds colleges and raises wages.", 0))
        return;
    act_propose(me());
}

static void cmd_scores(const char *title)
{
    const Game *g = &app.game;
    static char lines[16][120];
    static const char *ptr[17];
    int order[MAXPLAYERS], n = g->nplayers, k = 0;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && game_worth(g, order[j]) > game_worth(g, order[j - 1]); j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
    snprintf(lines[k++], 120, "%-18s %6s %6s %6s %6s %7s", "Player", "Cash", "Cards", "Houses", "Lux.", "Worth");
    snprintf(lines[k++], 120, "%-18s %6s %6s %6s %6s %7s", "------", "----", "-----", "------", "----", "-----");
    for (int i = 0; i < n; i++) {
        const Player *P = &g->pl[order[i]];
        int cards = 0;
        for (int sp = 0; sp < NSPACES; sp++)
            if (g->owner[sp] == order[i] && !(g->single_tax && board[sp].type == T_LOT)) cards++;
        snprintf(lines[k++], 120, "%-18.18s %6d %6d %6d %6d %7d", P->name, P->cash, cards,
                 game_houses_of(g, order[i]), P->luxuries + P->professors, game_worth(g, order[i]));
    }
    snprintf(lines[k++], 120, " ");
    snprintf(lines[k++], 120, "Every card, house and luxury counts 100%s.",
             g->single_tax ? " (lot title deeds count nothing under the Single Tax)" : "");
    snprintf(lines[k++], 120, "Turns played: %d.  Public Treasury: $%d.  Wages: $%d.", g->turn_no, g->treasury, g->wage);
    if (g->over) {
        snprintf(lines[k++], 120, " ");
        snprintf(lines[k++], 120, "%s is the winner - the millionaire!", g->pl[order[0]].name);
    }
    for (int i = 0; i < k; i++) ptr[i] = lines[i];
    ptr[k] = NULL;
    ui_info_box(title, ptr);
}

/* Click on a space: what it is. */
static void cmd_space_info(int sp)
{
    const Game *g = &app.game;
    const Space *s = &board[sp];
    char text[800], extra[300] = "";
    switch (s->type) {
    case T_LOT:
        snprintf(extra, sizeof extra, "A lot. Sale price $%d, land rent $%d, $10 more for each house.",
                 s->price, game_land_rent(g, sp));
        break;
    case T_RAILROAD:
        snprintf(extra, sizeof extra, "A railroad. Charter $%d. Fare now $%d.", s->price, game_rail_fare(g, sp));
        break;
    case T_FRANCHISE:
        snprintf(extra, sizeof extra, "A public utility held by private parties. Charter $%d. $5, or $25 with the "
                 "Municipal Cinch.", s->price);
        break;
    case T_TAX:
        snprintf(extra, sizeof extra, "An Absolute Necessity of life: $%d tax (indirect taxation).", game_tax(g));
        break;
    case T_TRESPASS:
        snprintf(extra, sizeof extra, g->colleges[sp] ? "A FREE COLLEGE stands here: stop for an Education card."
                                                      : "Land held out of use. Trespassers go to Jail.");
        break;
    case T_CHANCE:      snprintf(extra, sizeof extra, "Draw a card from the red pack."); break;
    case T_LUXURY:      snprintf(extra, sizeof extra, "Pay $75 for a luxury (counts 100), or move backward."); break;
    case T_SPECULATION: snprintf(extra, sizeof extra, "The broker's game. Broker's License $%d.", s->price); break;
    case T_LEGACY:      snprintf(extra, sizeof extra, "A legacy of $100."); break;
    case T_MOTHER:      snprintf(extra, sizeof extra, "Labor upon Mother Earth produces wages: $%d each time you reach or pass it.", g->wage); break;
    case T_JAIL:        snprintf(extra, sizeof extra, "Jail. $50 fine, or a double, to come out."); break;
    case T_PARK:        snprintf(extra, sizeof extra, "Central Park, maintained by public funds: free."); break;
    case T_POOR:        snprintf(extra, sizeof extra, "The Poor House, for those who cannot pay their way."); break;
    }
    int len = snprintf(text, sizeof text, "%s", extra);
    if (g->public_owned[sp]) len += snprintf(text + len, sizeof text - (size_t)len, "\n\nOwned by the public: free to all.");
    else if (board_has_card(sp)) {
        if (g->owner[sp] == NOBODY) len += snprintf(text + len, sizeof text - (size_t)len, "\n\nThe card is in the pack.");
        else len += snprintf(text + len, sizeof text - (size_t)len, "\n\nHeld by %s.", g->pl[g->owner[sp]].name);
    }
    if (g->houses[sp] && len < (int)sizeof text)
        snprintf(text + len, sizeof text - (size_t)len, " %d house%s, built by %s.", g->houses[sp],
                 g->houses[sp] == 1 ? "" : "s", g->pl[g->house_owner[sp]].name);
    int save = app.highlight;
    app.highlight = sp;
    ui_message(s->name, text);
    app.highlight = save;
}

static void cmd_about(void)
{
    ui_message("About Viola the Landlord",
               "Viola the Landlord " PACKAGE_VERSION_STR "\n\n"
               "The Landlord's Game, invented by Lizzie J. Magie (US Patent 748,626, 1904), played by the rules "
               "of 1906 with the Single Tax, against the computer, in the style of the Viola browser.\n\n"
               "Copyright (C) 2026 Lee Curtis. Free software under the GNU GPL, version 3 or later.");
}

/* ---- network play ---- */

static void leave_network(const char *why)
{
    if (net_role() != NET_NONE) net_close(why);
}

static void cmd_net_host(void)
{
    if (joiner()) { ui_message("Host a Network Game", "Leave the game you have joined first."); return; }
    if (app.playing && !app.game.over && !ui_yes_no("Host a Network Game", "Abandon the game in progress?", 0))
        return;
    leave_network("The host has started a new game.");
    start_network_game();
}

static void cmd_net_join(void)
{
    if (app.playing && !app.game.over && !ui_yes_no("Join a Network Game", "Abandon the game in progress?", 0))
        return;
    leave_network("The host has left the game.");
    app.playing = 0;
    netui_join();
}

static void roll(void)
{
    if (!app_can_act()) return;
    app.human_turn = 0;
    view_set_status("Rolling...");
    if (joiner()) { send_action(NX_ROLL, 0, 0, 0); return; }
    game_turn(&app.game);
}

static void run_command(Command c)
{
    switch (c) {
    case CMD_NEW_GAME:      cmd_new_game(); break;
    case CMD_ROLL:          roll(); break;
    case CMD_BUILD:         if (app_can_act()) cmd_build(); break;
    case CMD_TRADE_BUY:     if (app_can_act()) cmd_trade_buy(); break;
    case CMD_TRADE_SELL:    if (app_can_act()) cmd_trade_sell(); break;
    case CMD_SELL_PACK:     if (app_can_act()) cmd_sell_pack(); break;
    case CMD_SINGLE_TAX:    if (app_can_propose()) cmd_single_tax(); break;
    case CMD_SCORES:        if (app.playing) cmd_scores("Standings"); break;
    case CMD_NET_HOST:      cmd_net_host(); break;
    case CMD_NET_JOIN:      cmd_net_join(); break;
    case CMD_QUIT:
        if (!app.playing || app.game.over || ui_yes_no("Quit", "Leave the game in progress?", 0)) quit_requested = 1;
        break;
    case CMD_SETTINGS:      cmd_settings(); break;
    case CMD_SPEED:         cmd_speed(); break;
    case CMD_TOGGLE_BIDS:   app.prefs.ask_bids = !app.prefs.ask_bids; prefs_save(&app.prefs); break;
    case CMD_TOGGLE_CONFIRM: app.prefs.confirm_cpu = !app.prefs.confirm_cpu; prefs_save(&app.prefs); break;
    case CMD_HELP_PLAY:     ui_info_box("How to Play", help_play); break;
    case CMD_HELP_RULES:    ui_info_box("The Rules of 1906", help_rules); break;
    case CMD_HELP_HISTORY:  ui_info_box("Lizzie Magie and the Game", help_history); break;
    case CMD_HELP_NETWORK:  ui_info_box("Network Play", help_network); break;
    case CMD_HELP_KEYS:     ui_info_box("Keyboard", help_keys); break;
    case CMD_LICENSE:       ui_info_box("License", help_license); break;
    case CMD_ABOUT:         cmd_about(); break;
    case CMD_NONE:          break;
    }
}

/* The human's status line and focused button. */
static void update_status(void)
{
    const Game *g = &app.game;
    if (!app.playing) {
        view_set_status("Welcome. Game > New Game to begin.");
        view_set_focus(PB_NONE);
    } else if (g->over) {
        view_set_status("The game is over: %s wins.", g->pl[game_leader(g)].name);
        view_set_focus(PB_SCORES);
    } else if (app.human_turn) {
        const Player *P = &g->pl[g->cur];
        if (P->in_jail) view_set_status("%s's turn - in Jail. Roll the dice.", P->name);
        else view_set_status("%s's turn: build, trade, or roll the dice.", P->name);
        view_set_focus(PB_ROLL);
    } else if (joiner()) {
        view_set_status(g->cur == app.me ? "Waiting for the host..." : "%s is playing...", g->pl[g->cur].name);
        view_set_focus(PB_NONE);
    } else if (remote_seat(g->cur)) {
        view_set_status("%s is playing on his own machine...", g->pl[g->cur].name);
        view_set_focus(PB_NONE);
    } else {
        view_set_status("%s is thinking...", g->pl[g->cur].name);
        view_set_focus(PB_NONE);
    }
}

static void handle_key(int k)
{
    if (k == K_MOUSE) {
        if (xw_mouse.button == 4) { view_scroll_log(3); return; }
        if (xw_mouse.button == 5) { view_scroll_log(-3); return; }
        if (xw_mouse.button != 1) return;
        int m = menubar_hit(xw_mouse.x, xw_mouse.y);
        if (m >= 0) { run_command(menubar_run(m, 1, redraw)); return; }
        PanelButton b = view_button_at(xw_mouse.x, xw_mouse.y);
        if (b == PB_ROLL) run_command(CMD_ROLL);
        else if (b == PB_BUILD) run_command(CMD_BUILD);
        else if (b == PB_TRADE) {
            static const char *const items[] = { "&Buy a Card", "&Sell a Card", "Sell to the &Pack", "Cancel" };
            int i = ui_ask("Trade", "Trade with the other players, or sell back to the pack?", items, 4, 0, 3);
            if (i == 0) run_command(CMD_TRADE_BUY);
            else if (i == 1) run_command(CMD_TRADE_SELL);
            else if (i == 2) run_command(CMD_SELL_PACK);
        }
        else if (b == PB_SCORES) run_command(CMD_SCORES);
        else if (app.playing) {
            int sp = view_space_at(xw_mouse.x, xw_mouse.y);
            if (sp >= 0) cmd_space_info(sp);
        }
        return;
    }
    if (k == 27 || k == K_F(9) || k == K_F(10)) { run_command(menubar_run(MENU_GAME, 0, redraw)); return; }
    if (IS_ALT(k)) {
        int m = menubar_for_letter(k - K_ALT0);
        if (m >= 0) run_command(menubar_run(m, 1, redraw));
        return;
    }
    switch (k) {
    case K_CLOSE: run_command(CMD_QUIT); break;
    case 17:      run_command(CMD_QUIT); break;            /* Ctrl+Q */
    case 14:      run_command(CMD_NEW_GAME); break;        /* Ctrl+N */
    case ' ': case '\n': case 'r': case 'R':
        if (app.playing && app.game.over) run_command(CMD_SCORES);
        else run_command(CMD_ROLL);
        break;
    case 'b': case 'B': run_command(CMD_BUILD); break;
    case 't': case 'T': run_command(CMD_TRADE_BUY); break;
    case 's': case 'S': run_command(CMD_TRADE_SELL); break;
    case 'x': case 'X': run_command(CMD_SINGLE_TAX); break;
    case K_F(1):  run_command(CMD_HELP_PLAY); break;
    case K_F(2):  run_command(CMD_SCORES); break;
    case K_PGUP:  view_scroll_log(8); break;
    case K_PGDN:  view_scroll_log(-8); break;
    case K_END:   view_scroll_log(0); break;
    default: break;
    }
}

int main(int argc, char **argv)
{
    /* Started by a joiner's ssh on the host's machine: be the relay. */
    if (argc == 3 && !strcmp(argv[1], "--relay")) return net_relay(atoi(argv[2]));
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
            printf("viola-landlord %s\n", PACKAGE_VERSION_STR);
            return 0;
        }
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            printf("usage: viola-landlord [--version] [--help]\n"
                   "       viola-landlord --relay PORT   (run by ssh for network play; not by hand)\n"
                   "The Landlord's Game (Lizzie J. Magie, 1904) against the computer, for X11.\n"
                   "Settings are kept in ~/.config/viola-landlord/preferences.conf.\n");
            return 0;
        }
        fprintf(stderr, "viola-landlord: unknown option %s (try --help)\n", argv[i]);
        return 2;
    }
    prefs_load(&app.prefs);
    app.highlight = -1;
    if (!xw_open()) return 1;
    xw_show(argc, argv, 1180, 840);

    update_status();
    view_draw();
    xw_flush();
    cmd_new_game();                     /* straight to the New Game form */

    net_set_idle(netui_idle);
    while (!quit_requested) {
        Game *g = &app.game;
        /* A network joiner: the host plays the game; draw what it sends,
         * answer its questions, act on my turn. */
        if (joiner()) {
            if (app.playing && g->over && !final_shown) {
                final_shown = 1;
                update_status();
                cmd_scores("The Game Is Over");
                continue;
            }
            update_status();
            view_draw();
            xw_flush();
            int k = xw_wait_key(100);
            if (k != K_TIMEOUT && k != K_RESIZE) {
                if (k == K_CLOSE) {
                    if (ui_yes_no("Quit", "Leave the network game?", 0)) { net_close("A player has left."); quit_requested = 1; }
                } else {
                    handle_key(k);
                }
            }
            netui_client_pump(0);
            continue;
        }
        /* The host: keep the joiners' boards up to date (only changes are sent). */
        if (net_role() == NET_HOST && app.playing) net_broadcast(g);
        /* The host: a joiner's turn is played through net_decider. */
        if (app.playing && !g->over && remote_seat(g->cur)) {
            app.human_turn = 0;
            update_status();
            view_draw();
            xw_flush();
            game_turn(g);
            continue;
        }
        if (app.playing && !g->over && g->pl[g->cur].cpu) {
            app.human_turn = 0;
            app.cpu_running = 1;
            update_status();
            game_turn(g);
            app.cpu_running = 0;
            continue;
        }
        if (app.playing && !g->over) {
            app.human_turn = 1;
            /* Hot-seat: when the screen passes from one human to another,
             * say so, so nobody answers someone else's questions. */
            if (g->cur != last_human && humans_in_game(g) > 1) {
                char msg[120];
                update_status();
                snprintf(msg, sizeof msg, "Pass the game to %s.\n\nIt is %s's turn.", g->pl[g->cur].name,
                         g->pl[g->cur].name);
                ui_message("Next Player", msg);
            }
            last_human = g->cur;
        }
        if (app.playing && g->over && !final_shown) {
            final_shown = 1;
            app.human_turn = 0;
            update_status();
            cmd_scores("The Game Is Over");
            continue;
        }
        update_status();
        view_draw();
        xw_flush();
        xw_drain();
        int k;
        if (net_role() == NET_HOST) {
            /* Keep answering the network: turn latecomers away, notice who leaves. */
            k = xw_wait_key(200);
            net_host_poll(0);
            if (k == K_TIMEOUT) continue;
        } else {
            k = xw_next_key();
        }
        if (k == K_RESIZE) continue;
        handle_key(k);
    }
    if (net_role() != NET_NONE) net_close(joiner() ? "A player has left." : "The host has quit.");
    xw_close();
    return 0;
}
