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

/* netui.c - see netui.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netui.h"
#include "app.h"
#include "human.h"
#include "keys.h"
#include "net.h"
#include "prefs.h"
#include "ui.h"
#include "util.h"
#include "view.h"
#include "xwin.h"

/* ---- keeping the window alive ---- */

static int in_idle;             /* (a dialog opened while idling must not idle again) */

/* See netui.h: called while waiting on the network; Close asks whether to
 * leave. */
int netui_idle(void)
{
    if (in_idle) return 0;
    in_idle = 1;
    int quit = 0;
    int k = xw_wait_key(1);
    if (k == K_RESIZE) { view_draw(); xw_flush(); }
    if (k == K_CLOSE)
        quit = ui_yes_no("Leave the Game", net_role() == NET_HOST
                             ? "Players are connected to this game. End the network game for everyone?"
                             : "Leave the network game?", 0);
    in_idle = 0;
    return quit;
}

/* ---- hosting ---- */

/* How joiners can reach this machine: the host name and the IP addresses,
 * e.g. "lee@gamebox or lee@192.168.1.20 / 2001:db8::5". */
static void this_address(char *out, size_t size)
{
    char host[256] = "this-machine", ips[400];
    gethostname(host, sizeof host - 1);
    const char *u = getenv("USER");
    char user[64] = "";
    if (u && *u) snprintf(user, sizeof user, "%s@", u);
    if (net_local_addresses(ips, sizeof ips) > 0) {
        /* "a, b, c" -> "a / b / c", so the commas of the sentence stay clear */
        char shown[500];
        size_t o = 0;
        for (const char *q = ips; *q && o + 4 < sizeof shown; q++) {
            if (q[0] == ',' && q[1] == ' ') { memcpy(shown + o, " / ", 3); o += 3; q++; }
            else shown[o++] = *q;
        }
        shown[o] = '\0';
        snprintf(out, size, "%s%s (or the IP address: %s)", user, host, shown);
    } else {
        snprintf(out, size, "%s%s", user, host);
    }
}

/* The lobby: who has joined, until Start or Cancel. */
static int lobby(void)
{
    char addr[800], last_event[200] = "", here[256] = "this machine";
    this_address(addr, sizeof addr);
    gethostname(here, sizeof here - 1);
    for (;;) {
        net_host_poll(0);
        for (const char *e; (e = net_host_event()); ) copy_str(last_event, sizeof last_event, e);
        int joined = net_lobby_count();

        int lh = xw_line_height(XF_UI);
        int w = MIN(620, xw_width() - 40);
        char note[1400], buf[1600];
        const char *lines[16];
        snprintf(note, sizeof note,
                 "Players join from their own machines with Join a Network Game, entering %s%s "
                 "(a name works if their machine finds it in DNS or its hosts file). The game is reached through "
                 "ssh; see Help > Network Play. Four players at most: a fifth is turned away. If fewer than four "
                 "humans play, one computer player takes a free seat.",
                 addr, app.prefs.net_port == NET_PORT ? "" : " with the port after a colon");
        int nn = ui_wrap(XF_UI, note, w - 2 * UI_MARGIN, buf, sizeof buf, lines, 16);
        int rows = MAXPLAYERS + 2;
        int h = 2 * UI_MARGIN + lh + 12 + nn * lh + 12 + rows * (lh + 6) + 14 + UI_BUTTON_H;
        UiRect c = ui_frame("Waiting for Players", w, h);
        int y = c.y;
        for (int i = 0; i < nn; i++, y += lh)
            xw_text(XF_UI, c.x, y + xw_ascent(XF_UI), lines[i], (int)strlen(lines[i]), XC_INK);
        y += 12;
        UiRect list = { c.x, y, c.w, MAXPLAYERS * (lh + 6) + 6 };
        xw_fill(list.x, list.y, list.w, list.h, XC_SURFACE);
        xw_sunken(list.x, list.y, list.w, list.h);
        for (int i = 0; i < MAXPLAYERS; i++) {
            char row[400];
            if (i == 0) snprintf(row, sizeof row, "1.  %s  (host, this screen: %s)", app.prefs.names[0], here);
            else if (i <= joined) snprintf(row, sizeof row, "%d.  %s  (from %s)", i + 1, net_lobby_name(i - 1),
                                           net_lobby_from(i - 1));
            else if (i == joined + 1) snprintf(row, sizeof row, "%d.  a computer player (%s), unless someone joins", i + 1,
                                               app.prefs.net_cpu == SEAT_EASY ? "easy" : app.prefs.net_cpu == SEAT_HARD ? "hard" : "medium");
            else snprintf(row, sizeof row, "%d.  (open)", i + 1);
            xw_text(XF_UI, list.x + 10, list.y + 4 + i * (lh + 6) + xw_ascent(XF_UI), row, (int)strlen(row), XC_INK);
        }
        y = list.y + list.h + 6;
        if (last_event[0]) {
            xw_fill(c.x, y, c.w, lh + 4, XC_MARK);
            xw_text(XF_UI, c.x + 6, y + 2 + xw_ascent(XF_UI), last_event, (int)strlen(last_event), XC_INK);
        }
        UiRect start = { c.x + c.w - 2 * UI_BUTTON_W - 40, c.y + c.h - UI_BUTTON_H, UI_BUTTON_W + 30, UI_BUTTON_H };
        UiRect cancel = { c.x + c.w - UI_BUTTON_W, c.y + c.h - UI_BUTTON_H, UI_BUTTON_W, UI_BUTTON_H };
        ui_button(start, "&Start Game", joined > 0);
        ui_button(cancel, "Cancel", joined == 0);
        if (!joined) {          /* grayed until someone joins */
            xw_fill(start.x + 3, start.y + 3, start.w - 6, start.h - 6, XC_SURFACE);
            const char *t = "Start Game";
            int tw = xw_text_width(XF_UI, t, (int)strlen(t));
            xw_text(XF_UI, start.x + (start.w - tw) / 2, start.y + (start.h - lh) / 2 + xw_ascent(XF_UI), t,
                    (int)strlen(t), XC_TROUGH);
        }
        xw_flush();

        int k = xw_wait_key(150);
        if (k == K_TIMEOUT || k == K_RESIZE) continue;
        int go = 0, stop = 0;
        if (k == K_MOUSE && xw_mouse.button == 1) {
            if (ui_hit(start, xw_mouse.x, xw_mouse.y)) go = 1;
            if (ui_hit(cancel, xw_mouse.x, xw_mouse.y)) stop = 1;
        }
        if (k == 's' || k == 'S' || k == '\n') go = 1;
        if (k == 27 || k == K_CLOSE) stop = 1;
        if (go && !joined) { xw_bell(); continue; }
        if (go) return 1;
        if (stop) return 0;
    }
}

/* See netui.h: the computer player, length, rules and port; then the
 * waiting room. */
int netui_host_setup(GameSettings *s)
{
    static const char *const cpus[] = { "Easy", "Medium", "Hard" };
    static const char *const lens[] = { "3", "5", "7", "10" };
    static const char *const rules[] = { "Standard", "Advanced" };
    static const int len_val[] = { 3, 5, 7, 10 };
    UiRow rows[] = {
        { "Computer player", cpus, 3 },
        { "Game ends at wages", lens, 4 },
        { "Rules", rules, 2 },
    };
    int sel[3] = { CLAMP(app.prefs.net_cpu, SEAT_EASY, SEAT_HARD) - SEAT_EASY, 1, app.prefs.advanced };
    for (int i = 0; i < 4; i++) if (len_val[i] == app.prefs.end_wages) sel[1] = i;
    if (!ui_form("Host a Network Game",
                 "You host the game; up to three others join from their machines over ssh. One computer player "
                 "fills a free seat when fewer than four humans play.", rows, 3, sel, "&Open"))
        return 0;
    app.prefs.net_cpu = SEAT_EASY + sel[0];
    app.prefs.end_wages = len_val[sel[1]];
    app.prefs.advanced = sel[2];
    int port = app.prefs.net_port;
    if (!ui_number("Host a Network Game",
                   "The port this game listens on. Only this machine can reach it: players come in through ssh. "
                   "1904 unless that is taken.", &port))
        return 0;
    if (port <= 0 || port > 65535) { ui_message("Host a Network Game", "A port is a number from 1 to 65535."); return 0; }
    app.prefs.net_port = port;
    prefs_save(&app.prefs);

    char err[400];
    if (net_host(port, err, sizeof err) < 0) { ui_message("Host a Network Game", err); return 0; }
    net_set_host_name(app.prefs.names[0]);
    if (!lobby()) { net_close("The host has cancelled the game."); return 0; }
    memset(s, 0, sizeof *s);
    s->advanced = app.prefs.advanced;
    s->end_wages = app.prefs.end_wages;
    net_build_table(s, app.prefs.names[0], prefs_seat_difficulty(app.prefs.net_cpu));
    return 1;
}

/* ---- joining ---- */

static int client_started;      /* the host has sent START */

/* The host has started the game: a fresh board to draw. */
static void client_start(int me, int nplayers)
{
    Game *g = &app.game;
    memset(g, 0, sizeof *g);
    g->nplayers = CLAMP(nplayers, 2, MAXPLAYERS);
    for (int i = 0; i < NSPACES; i++) { g->owner[i] = NOBODY; g->house_owner[i] = NOBODY; }
    g->trust_a = g->trust_b = -1;
    g->wage = 100;
    app.me = CLAMP(me, 0, g->nplayers - 1);
    app.playing = 1;
    app.human_turn = 0;
    app.highlight = -1;
    client_started = 1;
}

/* The connection is gone: what is on the board stays, but nothing moves. */
static void client_lost(const char *why)
{
    char msg[600];
    snprintf(msg, sizeof msg, "%s", why && *why ? why : "The connection to the host was lost.");
    net_close(NULL);
    app.human_turn = 0;
    if (app.playing && !app.game.over) {
        app.game.over = 1;
        game_log(&app.game, "Disconnected: %s", msg);
    }
    ui_message("Network Game", msg);
}

/* A question from the host: the same dialog a player at the host's screen
 * would see, on my copy of the game. */
static int answer(const NetMsg *m)
{
    Game *g = &app.game;
    const Decider *d = &human_decider;
    int p = app.me, a = m->num[2], b = m->num[3], c = m->num[4];
    switch (m->num[1]) {
    case NA_DEAL_BUY: {
        int cards[NSPACES], want[NSPACES], n = 0;
        const char *t = m->text;
        while (*t && n < NSPACES) {
            char *end;
            long v = strtol(t, &end, 10);
            if (end == t) break;
            if (v >= 0 && v < NSPACES) cards[n++] = (int)v;
            t = end;
        }
        memset(want, 0, sizeof want);
        d->deal_buy(g, p, cards, n, want);
        int mask = 0;
        for (int i = 0; i < n && i < 30; i++) if (want[i]) mask |= 1 << i;
        return mask;
    }
    case NA_BUY:        return d->buy(g, p, a, b, c);
    case NA_MAX_BID:    return d->max_bid(g, p, a, b);
    case NA_JAIL_PAY:   return d->jail_pay(g, p);
    case NA_USE_PASS:   return d->use_pass(g, p, a, b);
    case NA_DIRECTION:  return d->direction(g, p, a, b);
    case NA_SPECULATE:  return d->speculate(g, p);
    case NA_LUXURY:     return d->luxury(g, p);
    case NA_ST_BUILD:   return d->st_build(g, p, a);
    case NA_VOTE:       return d->vote_single_tax(g, p, a);
    case NA_TRUST:      return d->join_trust(g, p, a);
    case NA_ACCEPT_SALE:     return human_accepts_sale(g, p, a, b, c);
    case NA_ACCEPT_PURCHASE: return human_accepts_purchase(g, p, a, b, c);
    case NA_RAISE: {
        int kind, sp, buyer;
        if (!human_raise_choice(g, p, a, &kind, &sp, &buyer)) return 0;
        return net_sale_encode(kind, sp, buyer);
    }
    default:
        return 0;
    }
}

/* One message from the host. */
static void client_message(const NetMsg *m)
{
    Game *g = &app.game;
    switch (m->kind) {
    case NM_START:
        client_start(m->num[0], m->num[1]);
        break;
    case NM_PLAYER: {
        int p = m->num[0];
        if (p < 0 || p >= MAXPLAYERS) break;
        /* "name|machine": the machine is shown next to the name. */
        char text[NET_LINE_MAX];
        copy_str(text, sizeof text, m->text);
        char *bar = strchr(text, '|');
        if (bar) *bar++ = '\0';
        copy_str(g->pl[p].name, NAME_LEN, text);
        net_set_player_from(p, bar ? bar : "");
        g->pl[p].cpu = m->num[1];
        g->pl[p].difficulty = m->num[2];
        break;
    }
    case NM_STATE: {
        char names[MAXPLAYERS][NAME_LEN];
        for (int p = 0; p < MAXPLAYERS; p++) copy_str(names[p], NAME_LEN, g->pl[p].name);
        if (net_decode_state(g, m->num, m->nnum) == 0)
            for (int p = 0; p < MAXPLAYERS; p++) copy_str(g->pl[p].name, NAME_LEN, names[p]);
        if (g->cur != app.me || g->over) app.human_turn = 0;
        break;
    }
    case NM_LOG:
        game_log(g, "%s", m->text);
        view_scroll_log(0);
        break;
    case NM_TURN:
        app.human_turn = m->num[0] == app.me && !g->over;
        if (app.human_turn) xw_bell();
        break;
    case NM_ASK: {
        view_draw();
        xw_flush();
        int ans[2] = { m->num[0], answer(m) };
        net_client_send(NM_ANSWER, ans, 2, NULL);
        break;
    }
    case NM_NOTE: {
        char title[128];
        const char *bar = strchr(m->text, '|');
        size_t n = bar ? (size_t)(bar - m->text) : 0;
        snprintf(title, sizeof title, "%.*s", (int)MIN(n, sizeof title - 1), m->text);
        ui_message(bar ? title : "Message", bar ? bar + 1 : m->text);
        break;
    }
    case NM_BYE:
        client_lost(m->text[0] ? m->text : "The host has ended the game.");
        break;
    default:
        break;
    }
}

/* See netui.h: handle up to a few hundred messages that have arrived. */
void netui_client_pump(int ms)
{
    NetMsg m;
    for (int i = 0; i < 400 && net_role() == NET_CLIENT; i++) {
        int r = net_client_recv(&m, i ? 0 : ms);
        if (r < 0) { client_lost("The connection to the host was lost."); return; }
        if (r == 0) return;
        client_message(&m);
    }
}

/* The waiting room: until the host starts (1) or someone gives up (0). */
static int waiting_room(void)
{
    char who[400] = "";
    client_started = 0;
    for (;;) {
        NetMsg m;
        int r;
        while ((r = net_client_recv(&m, 0)) > 0) {
            if (m.kind == NM_LOBBY) copy_str(who, sizeof who, m.text);
            else if (m.kind == NM_BYE || m.kind == NM_REFUSE) {
                net_close(NULL);
                ui_message("Network Game", m.text[0] ? m.text : "The host has closed the game.");
                return 0;
            } else {
                client_message(&m);
                if (client_started) return 1;
            }
        }
        if (r < 0) {
            net_close(NULL);
            ui_message("Network Game", "The connection to the host was lost.");
            return 0;
        }
        char text[600];
        snprintf(text, sizeof text, "You are in. Waiting for the host to start the game...\n\nAt the table: %s",
                 who[0] ? who : "(the host)");
        int lh = xw_line_height(XF_UI);
        char buf[800];
        const char *lines[12];
        int w = MIN(560, xw_width() - 40);
        int nl = ui_wrap(XF_UI, text, w - 2 * UI_MARGIN, buf, sizeof buf, lines, 12);
        UiRect c = ui_frame("Joined", w, 2 * UI_MARGIN + lh + 12 + nl * lh + 18 + UI_BUTTON_H);
        for (int i = 0; i < nl; i++)
            xw_text(XF_UI, c.x, c.y + i * lh + xw_ascent(XF_UI), lines[i], (int)strlen(lines[i]), XC_INK);
        UiRect leave = { c.x + c.w - UI_BUTTON_W, c.y + c.h - UI_BUTTON_H, UI_BUTTON_W, UI_BUTTON_H };
        ui_button(leave, "&Leave", 1);
        xw_flush();
        int k = xw_wait_key(150);
        if (k == 27 || k == K_CLOSE || k == '\n' || k == 'l' || k == 'L' ||
            (k == K_MOUSE && xw_mouse.button == 1 && ui_hit(leave, xw_mouse.x, xw_mouse.y))) {
            net_close("A player left before the game began.");
            return 0;
        }
    }
}

/* See netui.h: the host's address, ssh, then the waiting room. */
int netui_join(const char *given)
{
    char typed[192], target[256], host[256], err[1200];
    int port = NET_PORT;
    copy_str(typed, sizeof typed, given && *given ? given : app.prefs.net_host);
    for (;;) {
        if (!given || !*given) {
            if (!ui_prompt("Join a Network Game",
                           "The host's IP address or host name (a name must be in DNS or in this machine's "
                           "/etc/hosts). Put user@ in front to log in as another user, and :port after it if "
                           "the host chose another port than 1904 (for IPv6, [address]:port). You need "
                           "password-less ssh access to the host (see Help > Network Play).",
                           typed, sizeof typed) || !typed[0])
                return 0;
        }
        given = NULL;                       /* (asked again after a mistake) */
        port = NET_PORT;
        if (net_parse_address(typed, target, sizeof target, host, sizeof host, &port, err, sizeof err) < 0) {
            ui_message("Join a Network Game", err);
            continue;
        }
        char found[512];
        if (net_resolve(host, found, sizeof found, err, sizeof err) < 0) {
            ui_message("Host Not Found", err);
            continue;
        }
        break;
    }
    copy_str(app.prefs.net_host, sizeof app.prefs.net_host, typed);
    prefs_save(&app.prefs);
    copy_str(host, sizeof host, target);    /* ssh gets user@host, as typed */

    /* A note on screen while ssh connects (it can take a few seconds). */
    char msg[300];
    snprintf(msg, sizeof msg, "Connecting to %s through ssh...", host);
    int lh = xw_line_height(XF_UI);
    UiRect c = ui_frame("Join a Network Game", 460, 2 * UI_MARGIN + lh + 12 + lh);
    xw_text(XF_UI, c.x, c.y + xw_ascent(XF_UI), msg, (int)strlen(msg), XC_INK);
    xw_flush();

    if (net_join(host, port, app.prefs.net_command, app.prefs.names[0], err, sizeof err) < 0) {
        /* The table-full warning comes back here too. */
        ui_message(strstr(err, "table is full") ? "The Table Is Full" : "Join a Network Game", err);
        return 0;
    }
    return waiting_room();
}
