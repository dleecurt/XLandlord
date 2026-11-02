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

/* net.h - network play over ssh: the transport, the protocol and the network
 * seat. No X here (netui.c holds the dialogs), so the tests link it alone.
 *
 * THE MODEL: ONE HOST, UP TO THREE JOINERS
 *   One player hosts: his program runs the only real Game (game.c) and
 *   listens for players on 127.0.0.1 only - never on the network. The others
 *   join over ssh: the joiner's program runs
 *
 *       ssh -T -o BatchMode=yes [user@]host XLandlord --relay PORT
 *
 *   and the relay, a copy of XLandlord started by ssh on the host,
 *   connects to the hosting game on localhost and copies bytes both ways.
 *   ssh does the authentication and the encryption; the game never opens a
 *   port to the outside. (Password-less ssh keys are assumed: see README.)
 *
 *     joiner's game <--pipe--> ssh ====network==== sshd --> relay <--TCP 127.0.0.1--> host game
 *
 *   The table holds four players at most: the host and up to three joiners,
 *   or - when the host only hosts (net_set_host_plays(0)) - four joiners,
 *   the host watching from his screen without a seat.
 *   A fifth human is turned away with a warning (REFUSE). When fewer than four
 *   humans sit down, ONE computer player, at the skill the host chose, takes
 *   a free seat; with four humans there is no computer player. Joiners have
 *   no say in the computer player or its skill.
 *
 *   Once the game has started the table is closed: nobody new joins, and the
 *   computer player stays to the end. A human may leave (or his connection
 *   drop); he is out of the game - his cards go back into the pack and his
 *   houses come down (game_player_leaves) - and nobody takes his seat.
 *
 *   TIMERS (the host sets both when he opens the game; 1, 2 or 5 minutes):
 *   - Reconnecting. If a joiner's connection drops (not a BYE: that is a
 *     deliberate goodbye), his seat waits for him. His game retries ssh by
 *     itself; whoever comes back with the same name from the same machine
 *     gets his seat again (HELLO is answered by WELCOME, START, PLAYERs,
 *     STATE) and the question he was being asked is asked again. When the
 *     game needs him while he is away it pauses, and everyone sees how long
 *     is left (WAIT). If the time runs out he leaves the game.
 *   - Answering. A joiner has the answer time to answer a question or to
 *     finish his turn. Then his own game asks "Are you still there?" for
 *     NET_WARN_SECS seconds; "I'm still here" sends ALIVE, which starts his
 *     time again. The host gives up NET_WARN_SECS + NET_GRACE_SECS seconds
 *     after the answer time, and he leaves the game (BYE tells him why).
 *   - The host may remove a player he is waiting for at any time.
 *   If the host leaves, the game is over for everyone: BYE says so; a
 *   joiner who finds the host's game gone while reconnecting is told so.
 *
 *   The host's engine asks each remote seat its questions through
 *   net_decider: the question goes out as ASK and the engine waits for the
 *   ANSWER, exactly as it waits for a dialog. Joiners hold no rules: they draw
 *   the STATE the host sends after every change and answer ASKs with the same
 *   dialogs a local player sees. On his turn a joiner's builds, trades, sales,
 *   proposals and his throw come to the host as ACTIONs.
 *
 * THE PROTOCOL: one message per line, plain ASCII, fields separated by single
 * spaces; numbers first, then (for some kinds) a free text to the end.
 *
 *   relay -> host   FROM <ip address>               where the joiner's ssh came from
 *                                                   (sshd's SSH_CONNECTION); always the
 *                                                   first line, written by the relay itself
 *   joiner -> host  HELLO <protocol> <name>         ask for a seat
 *   host -> joiner  WELCOME <joined> <name of host>  you are in the lobby
 *   host -> joiner  REFUSE <text>                    turned away (table full...)
 *   host -> all     LOBBY <text>                     who is waiting, for display
 *   host -> all     START <you> <nplayers> <reconnect> <answer> <hostseat>
 *                                                    the game begins (or he is back);
 *                                                    you are player <you>; the timers
 *                                                    in seconds; the host's seat, or
 *                                                    -1 if he only hosts
 *   host -> all     PLAYER <p> <cpu> <diff> <name>|<from>   a player's name, kind and
 *                                                   machine (host name, or IP address)
 *   host -> all     STATE <numbers...>               the whole Game (net_encode_state)
 *   host -> all     LOG <text>                       a line of the record of events
 *   host -> all     TURN <p>                         it is player p's turn to act
 *   host -> one     ASK <id> <kind> <a> <b> <c> <text>   a question (NetAsk)
 *   joiner -> host  ANSWER <id> <value>              the reply
 *   joiner -> host  ACTION <kind> <a> <b> <c>        on his turn (NetAction)
 *   host -> one     NOTE <title>|<text>              a message to show in a dialog
 *   either          BYE <text>                       leaving / the game is over
 *   joiner -> host  ALIVE                            "I'm still here": his answer time
 *                                                    starts again
 *   host -> all     WAIT <p> <why> <secs>            waiting for player p (NW_*), secs
 *                                                    left; why 0: no longer waiting */
#ifndef NET_H
#define NET_H

#include <stddef.h>

#include "game.h"

#define NET_PORT        1904    /* the year of the patent */
#define NET_PROTOCOL    7
#define NET_LINE_MAX    4096
#define NET_MAX_FIELDS  320
#define NET_MAX_REMOTE  MAXPLAYERS          /* four joiners when the host only hosts */

typedef enum { NET_NONE, NET_HOST, NET_CLIENT } NetRole;

typedef enum {
    NM_HELLO, NM_WELCOME, NM_REFUSE, NM_LOBBY, NM_START, NM_PLAYER, NM_STATE,
    NM_LOG, NM_TURN, NM_ASK, NM_ANSWER, NM_ACTION, NM_NOTE, NM_BYE, NM_FROM,
    NM_ALIVE, NM_WAIT,
    NM_COUNT, NM_BAD = -1
} NetMsgKind;

/* The questions sent in ASK (a, b, c as noted). */
typedef enum {
    NA_DEAL_BUY,        /* text: the cards dealt (space numbers); answer: bit i = buy card i */
    NA_BUY,             /* a = space, b = price, c = high bidder (-1) */
    NA_MAX_BID,         /* a = space, b = least bid */
    NA_JAIL_PAY,
    NA_USE_PASS,        /* a = with the pass, b = without */
    NA_DIRECTION,       /* a = forward, b = backward */
    NA_SPECULATE,
    NA_LUXURY,
    NA_ST_BUILD,        /* a = space */
    NA_RAISE,           /* a = amount needed; answer: net_sale_encode, 0 = give up */
    NA_VOTE,            /* a = proposer */
    NA_TRUST,           /* a = the other partner */
    NA_ACCEPT_SALE,     /* you are the seller: a = buyer, b = space, c = price */
    NA_ACCEPT_PURCHASE, /* you are the buyer: a = seller, b = space, c = price */
    NA_COUNT
} NetAsk;

/* The things a joiner does on his turn (a, b, c as noted). */
typedef enum {
    NX_ROLL,            /* throw the dice: ends his business */
    NX_BUILD,           /* a = space */
    NX_TRADE_BUY,       /* a = seller, b = space, c = price offered */
    NX_TRADE_SELL,      /* a = buyer, b = space, c = price asked */
    NX_SELL,            /* a = net_sale_encode (to the pack, a house, to a computer player) */
    NX_PROPOSE,         /* propose the Single Tax */
    NX_COUNT
} NetAction;

typedef struct {
    NetMsgKind kind;
    int nnum;
    int num[NET_MAX_FIELDS];
    char text[NET_LINE_MAX];
} NetMsg;

/* ---- the message format ---- */

const char *net_kind_name(NetMsgKind k);
int net_fields_of(NetMsgKind k);        /* numbers before the text (STATE: see below) */
int net_format(char *out, size_t size, NetMsgKind kind, const int *num, int nnum, const char *text);
int net_parse(const char *line, NetMsg *m);    /* 0, or -1 if malformed */

/* The whole Game as STATE numbers (names travel in PLAYER). Returns the
 * count written to num[] (at most max). Decoding fills a Game the joiner
 * draws; it returns 0, or -1 if the numbers do not fit. */
int net_encode_state(const Game *g, int *num, int max);
int net_decode_state(Game *g, const int *num, int n);

/* A sale chosen to raise money: kind 1 a house, 2 to player `buyer`, 3 to
 * the pack. Encoded in one number (never 0). */
int  net_sale_encode(int kind, int sp, int buyer);
void net_sale_decode(int v, int *kind, int *sp, int *buyer);

/* ---- common ---- */

NetRole net_role(void);
/* Close everything (BYE to the other side(s) first). */
void net_close(const char *why);
/* In a child process made by fork(): forget the parent's connections
 * without closing them for the parent (no BYE is sent). */
void net_after_fork(void);
/* Called while the program waits on the network, so the window stays alive
 * (netui.c sets it). It returns 1 if the player asked to stop waiting. */
void net_set_idle(int (*idle)(void));

/* The "Are you still there?" warning, and the host's margin after it. */
#define NET_WARN_SECS   60
#define NET_GRACE_SECS  10

/* Why the game is waiting for a player (WAIT, and the host's wait hook). */
enum { NW_NONE, NW_ANSWER, NW_RECONNECT };

/* ---- hosting ---- */

/* Listen on 127.0.0.1:port. 0, or -1 with a message in err. */
int  net_host(int port, char *err, size_t errsize);
/* The host's own name, shown to joiners in the lobby. */
void net_set_host_name(const char *name);
/* Whether the host plays (seat 0, the default) or only hosts: then all four
 * seats are for joiners, the computer player takes a free one, and the
 * host watches. Set before net_build_table. */
void net_set_host_plays(int plays);
int  net_host_plays(void);
/* The host's seat in the game: 0, or -1 when he only hosts (on a joiner's
 * machine, as START said; -1 outside network play). */
int  net_host_seat(void);
void net_set_client_host_seat(int p);
/* The timers of the hosted game, in seconds (see TIMERS above). */
void net_set_timers(int reconnect_secs, int answer_secs);
/* The host's allowance for the joiner's warning and its margin (normally
 * NET_WARN_SECS and NET_GRACE_SECS; the tests shorten them). */
void net_set_warning(int warn_secs, int grace_secs);
/* Called about ten times a second while the host waits for player p
 * (`why` NW_ANSWER or NW_RECONNECT, `secs` left before he is removed); it
 * keeps the window alive. Returns 1 to remove him now, 2 if the host wants
 * to end the game, else 0. netui.c sets it. */
void net_set_wait_hook(int (*hook)(int p, int why, int secs));
/* Seconds before away player p is given up, or 0 if he is not away. */
int  net_seat_away(int p);
/* Take player p out of the game now (a BYE with `why` if he is still
 * connected); he leaves the game (game_player_leaves). */
void net_remove_player(int p, const char *why);
/* Accept and greet joiners, notice the ones who left; wait at most ms.
 * Before the game starts joiners wait in the lobby (four humans at most,
 * counting the host); afterwards newcomers are turned away. Returns 1 if the
 * lobby changed. */
int  net_host_poll(int ms);
int  net_lobby_count(void);             /* joiners waiting */
const char *net_lobby_name(int i);
/* Where joiner i in the lobby connects from: the host name of his machine,
 * or its IP address when no name is known for it. */
const char *net_lobby_from(int i);

/* Which machine player p plays at: for a joiner, the name of the machine his
 * ssh came from (or its IP address); for the host's seat, this machine's
 * name; "" for computer players and when not playing over the network.
 * Joiners learn these from the host's PLAYER messages. */
const char *net_player_from(int p);
void net_set_player_from(int p, const char *from);

/* The label for an IP address: its host name (looked up in the hosts file
 * and DNS, as the system does), or the address itself if none is found. */
void net_host_label(const char *ip, char *out, size_t size);
/* Which messages the lobby sent recently ("Lee was turned away: ..."), for
 * the host's screen; NULL when none is new. */
const char *net_host_event(void);

/* Seat the table: the host first (name), the joiners in the order they came,
 * then ONE computer player of `cpu_difficulty` if fewer than four humans.
 * Fills s->nplayers, names, cpu flags and difficulties. */
void net_build_table(GameSettings *s, const char *host_name, int cpu_difficulty);
/* The game has been created from those settings: tell everyone (START,
 * PLAYER, STATE). From now on the remote seats use net_decider. */
void net_start_game(Game *g);
/* Is player p a joiner's seat (still connected or not)? Connected? */
int  net_seat_remote(int p);
int  net_seat_connected(int p);

/* Send STATE and the new LOG lines to every joiner (the host's hooks call
 * this after every change). */
void net_broadcast(Game *g);
/* Show a message on player p's screen. */
void net_note(int p, const char *title, const char *text);
/* Tell everyone whose turn it is. */
void net_turn(int p);

/* Ask remote seat p a question; returns the answer, or -1 if p is not
 * connected (or drops while thinking). */
int  net_ask(int p, NetAsk kind, int a, int b, int c, const char *text);

/* What the host does with a joiner's ACTION (main.c sets it). Return 1 when
 * the action ends his business (NX_ROLL). If not set, only NX_ROLL works. */
void net_set_action_handler(int (*fn)(Game *g, int p, const NetMsg *m));

/* The Decider for joiners' seats: questions go to their machines. Once a
 * joiner has left, the few questions still asked for his seat (he holds
 * nothing and no money) get the computer's harmless answers. */
extern const Decider net_decider;

/* ---- addresses ---- */

/* Split what a player typed for the host - an IP address or a host name,
 * with an optional user@ in front and an optional :port after:
 *
 *     192.168.1.20      lee@192.168.1.20:1905      gamebox      lee@gamebox:1905
 *     fe80::1 (IPv6, no port)      [2001:db8::5]:1905      lee@[2001:db8::5]
 *
 * into the ssh target (user@host, as ssh wants it: an IPv6 address without
 * brackets), the bare host and the port (`*port` is left alone when none is
 * given). Returns 0, or -1 with the reason in err. */
int net_parse_address(const char *in, char *target, size_t tsize, char *host, size_t hsize,
                      int *port, char *err, size_t errsize);

/* Look a host up the way the system does (getaddrinfo: the hosts file and
 * DNS, in the order /etc/nsswitch.conf gives). An IP address is accepted as
 * it is. Returns 0 and the addresses found ("192.168.1.20, 2001:db8::5") in
 * `found`, or -1 with a message telling the player what to do. */
int net_resolve(const char *host, char *found, size_t fsize, char *err, size_t errsize);

/* This machine's addresses that other machines can reach: IPv4 first, then
 * global IPv6; loopback and link-local addresses left out. Returns how many
 * (written to out as "192.168.1.20, 2001:db8::5"). */
int net_local_addresses(char *out, size_t size);

/* ---- joining ---- */

/* Connect through ssh to `host` - what the player typed, without the :port
 * (see net_parse_address) - and ask for a seat as `name`. The host name is
 * looked up first (net_resolve), so a name that is in neither DNS nor the
 * hosts file gets a clear message before ssh is started; ssh is then given
 * the name as typed, so its known_hosts entry for that name still applies. `remote_cmd` is the program to run there (normally
 * "XLandlord"). The environment variable XLANDLORD_SSH replaces
 * "ssh" (the tests use it). Returns 0 once WELCOMEd, or -1 with the reason
 * (ssh's own error, or the host's REFUSE) in err. */
int  net_join(const char *host, int port, const char *remote_cmd, const char *name,
              char *err, size_t errsize);
/* The next message from the host, waiting at most ms. 1 = got one, 0 = none
 * yet, -1 = the connection is closed. */
int  net_client_recv(NetMsg *m, int ms);
int  net_client_send(NetMsgKind kind, const int *num, int nnum, const char *text);
/* When the joiner last sent the host anything, on net_clock_ms's clock (a
 * millisecond clock that only goes forward). */
long net_client_last_send(void);
long net_clock_ms(void);

/* ---- the relay (XLandlord --relay PORT, started by ssh) ---- */

/* Connect to the game hosted on this machine and copy stdin to it and its
 * output to stdout until either side closes. Returns the exit status. */
int  net_relay(int port);

#endif
