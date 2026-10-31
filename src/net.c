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

/* net.c - see net.h. No X here: the tests link it without a display. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "ai.h"
#include "util.h"

/* ---- the message format ---- */

static const char *const kind_names[NM_COUNT] = {
    "HELLO", "WELCOME", "REFUSE", "STATE", "LOG", "ASK", "ANSWER",
    "ACTION", "TURN", "CHAT", "BYE",
};

/* Numbers before the text, per kind. STATE's numbers are counted by
 * net_encode_state (TODO); 0 here means "all fields are numbers". */
static const int kind_fields[NM_COUNT] = {
    [NM_HELLO] = 1,     /* protocol; text = name */
    [NM_WELCOME] = 2,   /* seat, nplayers */
    [NM_REFUSE] = 0,    /* text = why */
    [NM_STATE] = 0,
    [NM_LOG] = 0,       /* text */
    [NM_ASK] = 5,       /* id, kind, a, b, c; text = the question */
    [NM_ANSWER] = 2,    /* id, value */
    [NM_ACTION] = 4,    /* kind, a, b, c */
    [NM_TURN] = 1,      /* player */
    [NM_CHAT] = 0,      /* text */
    [NM_BYE] = 0,       /* text */
};

const char *net_kind_name(NetMsgKind k)
{
    return k >= 0 && k < NM_COUNT ? kind_names[k] : "?";
}

int net_fields_of(NetMsgKind k)
{
    return k >= 0 && k < NM_COUNT ? kind_fields[k] : 0;
}

int net_format(char *out, size_t size, NetMsgKind kind, const int *num, int nnum, const char *text)
{
    if (kind < 0 || kind >= NM_COUNT || !size) return -1;
    size_t at = 0;
    int n = snprintf(out, size, "%s", kind_names[kind]);
    if (n < 0 || (size_t)n >= size) return -1;
    at = (size_t)n;
    for (int i = 0; i < nnum; i++) {
        n = snprintf(out + at, size - at, " %d", num[i]);
        if (n < 0 || (size_t)n >= size - at) return -1;
        at += (size_t)n;
    }
    if (text) {
        if (at + 1 + strlen(text) >= size) return -1;
        out[at++] = ' ';
        for (const char *p = text; *p; p++)
            out[at++] = *p == '\n' || *p == '\r' ? ' ' : *p;
        out[at] = '\0';
    }
    return (int)at;
}

int net_parse(const char *line, int nnum, NetMsg *m)
{
    memset(m, 0, sizeof *m);
    m->kind = NM_BAD;
    if (nnum < 0 || nnum > NET_MAX_FIELDS) return -1;
    const char *p = line;
    char word[16];
    int w = 0;
    while (*p && *p != ' ' && *p != '\n' && *p != '\r') {
        if (w >= (int)sizeof word - 1) return -1;
        word[w++] = *p++;
    }
    word[w] = '\0';
    for (int k = 0; k < NM_COUNT; k++)
        if (!strcmp(word, kind_names[k])) m->kind = (NetMsgKind)k;
    if (m->kind == NM_BAD) return -1;
    for (int i = 0; i < nnum; i++) {
        if (*p != ' ') return -1;
        p++;
        char *end;
        long v = strtol(p, &end, 10);
        if (end == p || (*end && *end != ' ' && *end != '\n' && *end != '\r')) return -1;
        m->num[i] = (int)v;
        p = end;
    }
    m->nnum = nnum;
    if (*p == ' ') p++;
    size_t n = strcspn(p, "\r\n");
    if (n >= sizeof m->text) return -1;
    memcpy(m->text, p, n);
    m->text[n] = '\0';
    return 0;
}

/* ---- the connection: stubs ---- */

static NetRole role = NET_NONE;

static int not_yet(char *err, size_t errsize)
{
    snprintf(err, errsize,
             "Network play is not available in this version yet. The plan and the protocol are in "
             "src/net.h; for now, play with several people at this screen (New Game: set seats to Human).");
    return -1;
}

int net_host(int port, char *err, size_t errsize)
{
    (void)port;
    /* TODO: socket(AF_INET6, SOCK_STREAM), SO_REUSEADDR, bind to port,
     * listen; accept HELLOs into open SEAT_NETWORK seats. */
    return not_yet(err, errsize);
}

int net_join(const char *host, int port, const char *name, char *err, size_t errsize)
{
    (void)host; (void)port; (void)name;
    /* TODO: getaddrinfo, connect, send HELLO, wait for WELCOME or REFUSE. */
    return not_yet(err, errsize);
}

void net_close(void)
{
    /* TODO: send BYE to everyone and close the sockets. */
    role = NET_NONE;
}

NetRole net_role(void) { return role; }

int net_seat_connected(int p)
{
    (void)p;
    return 0;               /* TODO: the seat's socket is open */
}

int net_ask(int p, NetAsk kind, int a, int b, int c, const char *text)
{
    (void)kind; (void)a; (void)b; (void)c; (void)text;
    if (!net_seat_connected(p)) return -1;
    /* TODO: send ASK <id> <kind> <a> <b> <c> <text> to seat p, then
     * net_poll until its ANSWER <id> arrives (or it drops: return -1). */
    return -1;
}

void net_broadcast(const Game *g)
{
    (void)g;
    /* TODO: send STATE (net_encode_state) and the LOG lines added since the
     * last broadcast (g->log_count tells how many) to every client. */
}

/* ---- the network seat's Decider ----
 *
 * Each question goes to the remote player with net_ask; if the seat is not
 * connected (always, until the sockets exist) the computer decides. */

static void nd_deal_buy(Game *g, int p, const int *cards, int n, int *want)
{
    /* TODO: one ASK per card (a = space, b = price), or one ASK with the list
     * in its text. */
    int ok = 1;
    for (int i = 0; i < n && ok; i++) {
        int r = net_ask(p, NA_DEAL_BUY, cards[i], board[cards[i]].price, 0, board[cards[i]].name);
        if (r < 0) ok = 0; else want[i] = r != 0;
    }
    if (!ok) ai_decider.deal_buy(g, p, cards, n, want);
}

/* For the yes/no questions: ask, or let the computer answer. */
#define ASK_OR_AI(kind, a, b, c, text, ai_call) \
    do { int r_ = net_ask(p, kind, a, b, c, text); return r_ >= 0 ? r_ : (ai_call); } while (0)

static int nd_buy(Game *g, int p, int sp, int price, int hb)
{ ASK_OR_AI(NA_BUY, sp, price, hb, board[sp].name, ai_decider.buy(g, p, sp, price, hb)); }

static int nd_max_bid(Game *g, int p, int sp, int min)
{ ASK_OR_AI(NA_MAX_BID, sp, min, 0, board[sp].name, ai_decider.max_bid(g, p, sp, min)); }

static int nd_jail_pay(Game *g, int p)
{ ASK_OR_AI(NA_JAIL_PAY, g->pl[p].jail_turns, 0, 0, NULL, ai_decider.jail_pay(g, p)); }

static int nd_use_pass(Game *g, int p, int with, int without)
{ ASK_OR_AI(NA_USE_PASS, with, without, 0, NULL, ai_decider.use_pass(g, p, with, without)); }

static int nd_direction(Game *g, int p, int fwd, int back)
{ ASK_OR_AI(NA_DIRECTION, fwd, back, 0, NULL, ai_decider.direction(g, p, fwd, back)); }

static int nd_speculate(Game *g, int p)
{ ASK_OR_AI(NA_SPECULATE, 0, 0, 0, NULL, ai_decider.speculate(g, p)); }

static int nd_luxury(Game *g, int p)
{ ASK_OR_AI(NA_LUXURY, 0, 0, 0, NULL, ai_decider.luxury(g, p)); }

static int nd_st_build(Game *g, int p, int sp)
{ ASK_OR_AI(NA_ST_BUILD, sp, 0, 0, board[sp].name, ai_decider.st_build(g, p, sp)); }

static int nd_raise(Game *g, int p, int need)
{
    /* TODO: the remote player picks a sale; the host then performs it and
     * answers 1, or the player gives up (0). */
    ASK_OR_AI(NA_RAISE, need, 0, 0, NULL, ai_decider.raise(g, p, need));
}

static int nd_vote(Game *g, int p, int proposer)
{ ASK_OR_AI(NA_VOTE, proposer, 0, 0, NULL, ai_decider.vote_single_tax(g, p, proposer)); }

static int nd_trust(Game *g, int p, int other)
{ ASK_OR_AI(NA_TRUST, other, 0, 0, NULL, ai_decider.join_trust(g, p, other)); }

/* A remote player's own business arrives as ACTION messages in the host's
 * main loop (TODO); while the seat is not connected the computer does it. */
static void nd_pre_roll(Game *g, int p)
{
    if (!net_seat_connected(p)) ai_decider.pre_roll(g, p);
}

const Decider net_decider = {
    .deal_buy = nd_deal_buy,
    .buy = nd_buy,
    .max_bid = nd_max_bid,
    .jail_pay = nd_jail_pay,
    .use_pass = nd_use_pass,
    .direction = nd_direction,
    .speculate = nd_speculate,
    .luxury = nd_luxury,
    .st_build = nd_st_build,
    .raise = nd_raise,
    .vote_single_tax = nd_vote,
    .join_trust = nd_trust,
    .pre_roll = nd_pre_roll,
};
