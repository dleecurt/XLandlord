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

/* net.h - network play: THE PLAN, and the parts already in place.
 *
 * STATUS: a skeleton. The protocol's message format (net_format /
 * net_parse) and the network seat's Decider are written and tested; the
 * sockets, the lobby and the client's drawing are stubs that say so.
 * Everything marked TODO below is the work left.
 *
 * THE DESIGN: ONE GAME, ON THE HOST
 *   The host runs the only real Game (game.c), exactly as for hot-seat play.
 *   A seat taken by a player on another machine gets net_decider: each of
 *   its questions (buy? bid? pay the fine?...) is sent to that player's
 *   machine as an ASK message, and the engine waits for the ANSWER - the
 *   engine already waits for answers from dialogs, so game.c does not
 *   change at all. Clients hold no rules: after every change the host sends
 *   them the STATE and the new LOG lines, which they draw with view.c, and
 *   they answer ASKs with the same dialogs human.c shows at the host.
 *
 *     host                                   client (one per remote seat)
 *     ----                                   ------
 *     game.c --question--> net_decider --ASK--> dialog (human.c)
 *            <--answer---              <-ANSWER-
 *     hooks.changed ------------------STATE/LOG--> view_draw
 *     main loop <---------------------ACTION----- Roll / Build / Trade menus
 *
 * THE PROTOCOL
 *   TCP, default port NET_PORT. One message per line of plain ASCII,
 *   fields separated by single spaces, the last field may contain spaces
 *   (text). Lines are at most NET_LINE_MAX bytes.
 *
 *     client -> host   HELLO <protocol> <name>      ask for a seat
 *     host -> client   WELCOME <seat> <nplayers>    you are player <seat>
 *     host -> client   REFUSE <text>                no seat (full, wrong version)
 *     host -> client   STATE <fields...>            the whole Game (net_encode_state)
 *     host -> client   LOG <text>                   one line for the record of events
 *     host -> client   ASK <id> <kind> <a> <b> <c> <text>
 *                                                   a Decider question (NetAsk)
 *     client -> host   ANSWER <id> <value>          the reply to ASK <id>
 *     client -> host   ACTION <kind> <a> <b> <c>    on its turn: roll, build, trade...
 *     host -> client   TURN <player>                whose turn it is now
 *     either           CHAT <text>                  a line typed to the table
 *     either           BYE <text>                   leaving (or the host ending the game)
 *
 * WHEN A PLAYER DROPS (TODO): the host gives the seat to a computer player
 *   of medium strength and logs it; net_decider already falls back to
 *   ai_decider for any seat that is not connected, so the game never stalls.
 *
 * TODO, in order:
 *   1. net_host / net_join / net_close: sockets (socket, bind, listen,
 *      accept; connect), non-blocking, with line buffering.
 *   2. net_poll(ms): one select() over the X connection and the sockets, so
 *      the window stays alive while the engine waits for an ANSWER. xwin.c
 *      will need to expose its file descriptor (ConnectionNumber).
 *   3. net_encode_state / net_decode_state: every field of Game a client
 *      draws (players, owners, houses, treasury, wage, dice, flags).
 *   4. The lobby: Host a Network Game (port, seats left open for
 *      SEAT_NETWORK in the New Game form) and Join (host:port, name).
 *   5. Client mode in main.c: no engine; draw STATE, answer ASK, send
 *      ACTION from the Roll / Build / Trade commands.
 *   6. A make-check test running a host and clients over the loopback. */
#ifndef NET_H
#define NET_H

#include <stddef.h>

#include "game.h"

#define NET_PORT        1904    /* the year of the patent */
#define NET_PROTOCOL    1
#define NET_LINE_MAX    1024
#define NET_MAX_FIELDS  64

typedef enum { NET_NONE, NET_HOST, NET_CLIENT } NetRole;

/* The kinds of message (the first word of a line). */
typedef enum {
    NM_HELLO, NM_WELCOME, NM_REFUSE, NM_STATE, NM_LOG, NM_ASK, NM_ANSWER,
    NM_ACTION, NM_TURN, NM_CHAT, NM_BYE, NM_COUNT, NM_BAD = -1
} NetMsgKind;

/* The Decider questions, as sent in ASK. */
typedef enum {
    NA_DEAL_BUY, NA_BUY, NA_MAX_BID, NA_JAIL_PAY, NA_USE_PASS, NA_DIRECTION,
    NA_SPECULATE, NA_LUXURY, NA_ST_BUILD, NA_RAISE, NA_VOTE, NA_TRUST, NA_COUNT
} NetAsk;

/* The things a client does on its turn, as sent in ACTION. */
typedef enum {
    NX_ROLL, NX_BUILD, NX_TRADE_BUY, NX_TRADE_SELL, NX_SELL_PACK, NX_SELL_HOUSE,
    NX_PROPOSE_SINGLE_TAX, NX_COUNT
} NetAction;

/* A parsed message: its kind, its numeric fields and its text (the rest of
 * the line after the fixed fields). */
typedef struct {
    NetMsgKind kind;
    int nnum;
    int num[NET_MAX_FIELDS];
    char text[NET_LINE_MAX];
} NetMsg;

/* ---- the message format (written and tested) ---- */

const char *net_kind_name(NetMsgKind k);

/* Write one line, without the newline: the kind, `nnum` numbers, then the
 * text if not NULL (newlines in it become spaces). Returns its length, or
 * -1 if it does not fit in `size`. */
int net_format(char *out, size_t size, NetMsgKind kind, const int *num, int nnum, const char *text);

/* Parse a line (with or without its newline) that should carry `nnum`
 * numbers before its text. Returns 0, or -1 for a malformed line. */
int net_parse(const char *line, int nnum, NetMsg *m);

/* How many numbers each kind of message carries before its text. */
int net_fields_of(NetMsgKind k);

/* ---- the connection (stubs) ---- */

/* Start hosting on `port` / connect to host:port as `name`. Return 0 on
 * success, or -1 with a message for the player in `err`. Not implemented
 * yet: they always fail with an explanation. */
int  net_host(int port, char *err, size_t errsize);
int  net_join(const char *host, int port, const char *name, char *err, size_t errsize);
void net_close(void);
NetRole net_role(void);

/* Is seat p of the hosted game connected to a remote player? */
int  net_seat_connected(int p);

/* Send a Decider question to seat p and wait for the answer. Returns the
 * answer, or -1 if the seat is not connected (the caller then decides for
 * it). The stub returns -1. */
int  net_ask(int p, NetAsk kind, int a, int b, int c, const char *text);

/* Tell every client the game has changed (STATE and the new LOG lines). */
void net_broadcast(const Game *g);

/* The Decider for a seat played over the network. Until the seat's player
 * is connected, each question is answered by ai_decider, so a network seat
 * plays like a computer player of its difficulty. */
extern const Decider net_decider;

#endif
