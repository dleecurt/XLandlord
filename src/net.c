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

/* net.c - see net.h. Plain POSIX sockets and pipes; no X. */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "net.h"
#include "ai.h"
#include "util.h"

/* ==== the message format ==== */

static const char *const kind_names[NM_COUNT] = {
    "HELLO", "WELCOME", "REFUSE", "LOBBY", "START", "PLAYER", "STATE",
    "LOG", "TURN", "ASK", "ANSWER", "ACTION", "NOTE", "BYE",
};

/* Numbers before the text; -1: the whole line is numbers. */
static const int kind_fields[NM_COUNT] = {
    [NM_HELLO] = 1, [NM_WELCOME] = 1, [NM_REFUSE] = 0, [NM_LOBBY] = 0,
    [NM_START] = 2, [NM_PLAYER] = 3, [NM_STATE] = -1, [NM_LOG] = 0,
    [NM_TURN] = 1, [NM_ASK] = 5, [NM_ANSWER] = 2, [NM_ACTION] = 4,
    [NM_NOTE] = 0, [NM_BYE] = 0,
};

/* See net.h: the first word of a message line, and its count of numbers. */
const char *net_kind_name(NetMsgKind k) { return k >= 0 && k < NM_COUNT ? kind_names[k] : "?"; }
int net_fields_of(NetMsgKind k) { return k >= 0 && k < NM_COUNT ? kind_fields[k] : 0; }

/* See net.h: one message line, without its newline. */
int net_format(char *out, size_t size, NetMsgKind kind, const int *num, int nnum, const char *text)
{
    if (kind < 0 || kind >= NM_COUNT || !size) return -1;
    int n = snprintf(out, size, "%s", kind_names[kind]);
    if (n < 0 || (size_t)n >= size) return -1;
    size_t at = (size_t)n;
    for (int i = 0; i < nnum; i++) {
        n = snprintf(out + at, size - at, " %d", num[i]);
        if (n < 0 || (size_t)n >= size - at) return -1;
        at += (size_t)n;
    }
    if (text) {
        if (at + 1 + strlen(text) >= size) return -1;
        out[at++] = ' ';
        for (const char *p = text; *p; p++) out[at++] = *p == '\n' || *p == '\r' ? ' ' : *p;
        out[at] = '\0';
    }
    return (int)at;
}

/* See net.h: a line into a NetMsg (the kind's numbers, then the text). */
int net_parse(const char *line, NetMsg *m)
{
    memset(m, 0, sizeof *m);
    m->kind = NM_BAD;
    const char *p = line;
    char word[16];
    int w = 0;
    while (*p && *p != ' ' && *p != '\n' && *p != '\r') {
        if (w >= (int)sizeof word - 1) return -1;
        word[w++] = *p++;
    }
    word[w] = '\0';
    for (int k = 0; k < NM_COUNT; k++) if (!strcmp(word, kind_names[k])) m->kind = (NetMsgKind)k;
    if (m->kind == NM_BAD) return -1;
    int want = kind_fields[m->kind];
    for (int i = 0; want < 0 || i < want; i++) {
        if (want < 0 && (*p == '\0' || *p == '\n' || *p == '\r')) break;
        if (*p != ' ' || i >= NET_MAX_FIELDS) { m->kind = NM_BAD; return -1; }
        p++;
        char *end;
        long v = strtol(p, &end, 10);
        if (end == p || (*end && *end != ' ' && *end != '\n' && *end != '\r')) { m->kind = NM_BAD; return -1; }
        m->num[m->nnum++] = (int)v;
        p = end;
    }
    if (*p == ' ') p++;
    size_t n = strcspn(p, "\r\n");
    if (n >= sizeof m->text) { m->kind = NM_BAD; return -1; }
    memcpy(m->text, p, n);
    m->text[n] = '\0';
    return 0;
}

#define STATE_HEAD   15
#define STATE_PLAYER 12
#define STATE_SPACE  5

/* See net.h: the Game as numbers - a header, then each player, then each
 * space. */
int net_encode_state(const Game *g, int *num, int max)
{
    int need = STATE_HEAD + g->nplayers * STATE_PLAYER + NSPACES * STATE_SPACE, k = 0;
    if (need > max) return -1;
    int head[STATE_HEAD] = {
        g->nplayers, g->cur, g->turn_no, g->treasury, g->single_tax, g->st_progress, g->wage,
        g->trust_a, g->trust_b, g->trust_refused, g->advanced, g->end_wages, g->over, g->last_d1, g->last_d2,
    };
    for (int i = 0; i < STATE_HEAD; i++) num[k++] = head[i];
    for (int p = 0; p < g->nplayers; p++) {
        const Player *P = &g->pl[p];
        int f[STATE_PLAYER] = { P->cpu, P->difficulty, P->cash, P->pos, P->in_jail, P->jail_turns,
                                P->in_poorhouse, P->natural_opp, P->wages, P->luxuries, P->education, P->professors };
        for (int i = 0; i < STATE_PLAYER; i++) num[k++] = f[i];
    }
    for (int s = 0; s < NSPACES; s++) {
        num[k++] = g->owner[s];
        num[k++] = g->houses[s];
        num[k++] = g->house_owner[s];
        num[k++] = g->public_owned[s];
        num[k++] = g->colleges[s];
    }
    return k;
}

/* See net.h: the reverse of net_encode_state, with every value checked. */
int net_decode_state(Game *g, const int *num, int n)
{
    if (n < STATE_HEAD) return -1;
    int np = num[0];
    if (np < 2 || np > MAXPLAYERS || n != STATE_HEAD + np * STATE_PLAYER + NSPACES * STATE_SPACE) return -1;
    int k = 0;
    g->nplayers = num[k++];
    int cur = num[k++];
    g->cur = CLAMP(cur, 0, np - 1);
    g->turn_no = num[k++];
    g->treasury = num[k++]; g->single_tax = num[k++]; g->st_progress = num[k++]; g->wage = num[k++];
    g->trust_a = num[k++]; g->trust_b = num[k++]; g->trust_refused = num[k++]; g->advanced = num[k++];
    g->end_wages = num[k++]; g->over = num[k++]; g->last_d1 = num[k++]; g->last_d2 = num[k++];
    for (int p = 0; p < np; p++) {
        Player *P = &g->pl[p];
        P->cpu = num[k++]; P->difficulty = num[k++]; P->cash = num[k++];
        int pos = num[k++];
        P->pos = CLAMP(pos, 0, NSPACES - 1);
        P->in_jail = num[k++]; P->jail_turns = num[k++]; P->in_poorhouse = num[k++]; P->natural_opp = num[k++];
        P->wages = num[k++]; P->luxuries = num[k++]; P->education = num[k++]; P->professors = num[k++];
    }
    for (int s = 0; s < NSPACES; s++) {
        int o = num[k++];
        g->owner[s] = o >= 0 && o < np ? o : NOBODY;
        int h = num[k++];
        g->houses[s] = CLAMP(h, 0, 9);
        o = num[k++];
        g->house_owner[s] = o >= 0 && o < np ? o : NOBODY;
        g->public_owned[s] = num[k++] != 0;
        g->colleges[s] = num[k++] != 0;
    }
    return 0;
}

int net_sale_encode(int kind, int sp, int buyer) { return (kind & 3) | (sp << 2) | ((buyer + 1) << 8); }

/* See net.h: the reverse of net_sale_encode. */
void net_sale_decode(int v, int *kind, int *sp, int *buyer)
{
    *kind = v & 3;
    *sp = (v >> 2) & 63;
    *buyer = ((v >> 8) & 15) - 1;
}

/* ==== lines over a file descriptor ==== */

typedef struct {
    int fd;
    char buf[2 * NET_LINE_MAX];
    int len;
} LineBuf;

/* Non-blocking, and not inherited by ssh or anything else we start. */
static void set_nonblock(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
}

/* Write all of a line plus its newline. 0, or -1 if the other end is gone. */
static int write_line(int fd, const char *line)
{
    char out[NET_LINE_MAX + 2];
    size_t n = strlen(line);
    if (n > NET_LINE_MAX) n = NET_LINE_MAX;
    memcpy(out, line, n);
    out[n++] = '\n';
    size_t at = 0;
    while (at < n) {
        ssize_t w = write(fd, out + at, n - at);
        if (w > 0) { at += (size_t)w; continue; }
        if (w < 0 && (errno == EAGAIN || errno == EINTR)) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(fd, &set);
            struct timeval tv = { 5, 0 };
            if (select(fd + 1, NULL, &set, NULL, &tv) <= 0) return -1;
            continue;
        }
        return -1;
    }
    return 0;
}

/* Format a message and write it as a line. */
static int send_msg(int fd, NetMsgKind kind, const int *num, int nnum, const char *text)
{
    char line[NET_LINE_MAX];
    if (net_format(line, sizeof line, kind, num, nnum, text) < 0) return -1;
    return write_line(fd, line);
}

/* Read what has arrived. 0 = ok (maybe nothing), -1 = closed. */
static int fill(LineBuf *b)
{
    for (;;) {
        if (b->len >= (int)sizeof b->buf - 1) b->len = 0;   /* an absurd line: drop it */
        ssize_t r = read(b->fd, b->buf + b->len, sizeof b->buf - 1 - (size_t)b->len);
        if (r > 0) { b->len += (int)r; continue; }
        if (r == 0) return -1;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        if (errno == EINTR) continue;
        return -1;
    }
}

/* Take one complete line out of the buffer (1), or 0 if none yet. */
static int take_line(LineBuf *b, char *out, size_t size)
{
    char *nl = memchr(b->buf, '\n', (size_t)b->len);
    if (!nl) return 0;
    size_t n = (size_t)(nl - b->buf);
    size_t c = MIN(n, size - 1);
    memcpy(out, b->buf, c);
    out[c] = '\0';
    memmove(b->buf, nl + 1, (size_t)b->len - n - 1);
    b->len -= (int)(n + 1);
    return 1;
}

/* Milliseconds on the wall clock. */
static long now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

/* Wait until fd is readable or ms pass. 1 readable, 0 timeout. */
static int wait_readable(int fd, int ms)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    return select(fd + 1, &set, NULL, NULL, &tv) > 0;
}

/* ==== common ==== */

static NetRole role = NET_NONE;         /* hosting, joined, or neither */
static int (*idle_fn)(void);            /* keeps the window alive during waits (netui_idle) */

/* The joiner's side: the pipe to ssh, ssh itself, and where ssh's errors go. */
static LineBuf cin = { -1, "", 0 };
static pid_t ssh_pid = -1;
static FILE *ssh_err;
void net_client_close(const char *why);

/* See net.h: whether this program hosts, has joined, or neither; and the
 * function that keeps the window alive during network waits. */
NetRole net_role(void) { return role; }
void net_set_idle(int (*idle)(void)) { idle_fn = idle; }

static int idle(void) { return idle_fn ? idle_fn() : 0; }

/* ==== hosting ==== */

#define MAX_CONNS 8

typedef struct {
    LineBuf in;
    int open;
    int hello;                  /* sent HELLO and was let in */
    int player;                 /* seat in the game, or -1 */
    char name[NAME_LEN];
} Conn;

static int listen_fd = -1;              /* the localhost listener */
static Conn conns[MAX_CONNS];           /* everyone connected: in the lobby, seated, or just arrived */
static int lobby[NET_MAX_REMOTE], nlobby;       /* conn indices, in order of arrival */
static int lobby_open;                          /* before the game starts */
static int seat_conn[MAXPLAYERS];               /* conn of a remote seat, or -1 */
static int seat_remote[MAXPLAYERS];     /* the seat belongs to a joiner (even after he left) */
static Game *hgame;                     /* the hosted game, once started */
static int sent_log;                    /* log lines already sent to the joiners */
static int ask_id;                      /* number of the last ASK, to match its ANSWER */
static int waiting_conn = -1;
static int last_num[NET_MAX_FIELDS], last_n = -1;    /* the STATE last sent */                   /* the conn a wait reads itself */
static int in_broadcast;                /* (a broadcast must not start another) */
static char host_name[NAME_LEN];        /* shown to joiners in the lobby */
static int (*action_fn)(Game *, int, const NetMsg *);   /* main.c's host_action */
static char events[8][200];
static int nevents, events_read;

/* See net.h: what the host does with a joiner's ACTION. */
void net_set_action_handler(int (*fn)(Game *g, int p, const NetMsg *m)) { action_fn = fn; }

/* Remember something for the host's waiting room (a join, a refusal, a
 * departure). */
static void event(const char *fmt, const char *a, const char *b)
{
    snprintf(events[nevents % 8], sizeof events[0], fmt, a, b);
    nevents++;
}

/* See net.h: the oldest lobby event not yet shown. */
const char *net_host_event(void)
{
    if (events_read >= nevents) return NULL;
    if (nevents - events_read > 8) events_read = nevents - 8;
    return events[events_read++ % 8];
}

int net_lobby_count(void) { return nlobby; }
const char *net_lobby_name(int i) { return i >= 0 && i < nlobby ? conns[lobby[i]].name : ""; }
int net_seat_remote(int p) { return role == NET_HOST && p >= 0 && p < MAXPLAYERS && seat_remote[p]; }
int net_seat_connected(int p) { return net_seat_remote(p) && seat_conn[p] >= 0 && conns[seat_conn[p]].open; }

/* See net.h: listen on 127.0.0.1 only; joiners arrive through ssh and the
 * relay. */
int net_host(int port, char *err, size_t errsize)
{
    net_close(NULL);
    signal(SIGPIPE, SIG_IGN);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { snprintf(err, errsize, "Cannot make a socket: %s.", strerror(errno)); return -1; }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);     /* localhost only: joiners come through ssh */
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0 || listen(fd, 8) < 0) {
        snprintf(err, errsize, "Cannot listen on port %d of this machine: %s. (Is another game already "
                 "hosted here? Choose another port.)", port, strerror(errno));
        close(fd);
        return -1;
    }
    set_nonblock(fd);
    listen_fd = fd;
    role = NET_HOST;
    lobby_open = 1;
    nlobby = 0;
    for (int p = 0; p < MAXPLAYERS; p++) { seat_conn[p] = -1; seat_remote[p] = 0; }
    hgame = NULL;
    sent_log = 0;
    return 0;
}

/* Close a connection without any farewell. */
static void close_conn(int c)
{
    if (!conns[c].open) return;
    close(conns[c].in.fd);
    conns[c].open = 0;
}

/* Tell everyone waiting who is at the table. */
static void send_lobby(void)
{
    char text[400];
    int n = snprintf(text, sizeof text, "%s (host)", host_name[0] ? host_name : "Host");
    for (int i = 0; i < nlobby && n < (int)sizeof text; i++)
        n += snprintf(text + n, sizeof text - (size_t)n, ", %s", conns[lobby[i]].name);
    for (int i = 0; i < nlobby; i++) send_msg(conns[lobby[i]].in.fd, NM_LOBBY, NULL, 0, text);
}

/* A joiner has gone: out of the lobby, or out of the game (a computer
 * player takes the seat). */
static void drop(int c)
{
    if (!conns[c].open) return;
    char name[NAME_LEN];
    copy_str(name, sizeof name, conns[c].name);
    int p = conns[c].player;
    close_conn(c);
    for (int i = 0; i < nlobby; i++) {
        if (lobby[i] != c) continue;
        memmove(lobby + i, lobby + i + 1, (size_t)(nlobby - i - 1) * sizeof lobby[0]);
        nlobby--;
        event("%s has left the lobby.%s", name, "");
        send_lobby();
        return;
    }
    if (p >= 0 && hgame && seat_conn[p] == c) {
        seat_conn[p] = -1;
        hgame->pl[p].cpu = 1;
        hgame->pl[p].difficulty = DIFF_MEDIUM;
        event("%s has left the game.%s", name, "");
        game_log(hgame, "%s has left the game; a computer player (medium) takes the seat.", name);
    }
}

/* Turn a newcomer away with a REFUSE carrying the reason. */
static void refuse(int c, const char *why)
{
    send_msg(conns[c].in.fd, NM_REFUSE, NULL, 0, why);
    close_conn(c);
}

/* Humans at the table now: the host and the joiners (or, during a game, the
 * human seats). */
static int humans_seated(void)
{
    if (!hgame) return 1 + nlobby;
    int n = 0;
    for (int q = 0; q < hgame->nplayers; q++) n += !hgame->pl[q].cpu;
    return n;
}

#define FULL_MESSAGE "The table is full: four players are already seated (the host and three others), " \
                     "and four is the limit. Try again when the next game is set up."

/* A new conn's first line: HELLO, then in, or turned away. */
static void greet(int c, const char *line)
{
    NetMsg m;
    if (net_parse(line, &m) < 0 || m.kind != NM_HELLO) { refuse(c, "That is not Viola the Landlord speaking."); return; }
    copy_str(conns[c].name, sizeof conns[c].name, m.text[0] ? m.text : "Player");
    if (m.num[0] != NET_PROTOCOL) {
        event("%s was turned away: a different version of the game.%s", conns[c].name, "");
        refuse(c, "The host runs a different version of Viola the Landlord; use the same version on every machine.");
        return;
    }
    if (humans_seated() >= MAXPLAYERS) {
        event("%s was turned away: the table is full (four players).%s", conns[c].name, "");
        refuse(c, FULL_MESSAGE);
        return;
    }
    if (!lobby_open) {
        event("%s was turned away: a game is in progress.%s", conns[c].name, "");
        refuse(c, "A game is already being played on this host. Ask the host to set up a new one, then join.");
        return;
    }
    conns[c].hello = 1;
    conns[c].player = -1;
    lobby[nlobby++] = c;
    int num[1] = { nlobby };
    send_msg(conns[c].in.fd, NM_WELCOME, num, 1, host_name);
    event("%s has joined.%s", conns[c].name, "");
    send_lobby();
}

/* Read from conn c and act on the lines that are for the host's poll: a
 * HELLO from a newcomer, a BYE from anyone. Lines for a wait in progress
 * (waiting_conn) stay in the buffer. */
static void service(int c)
{
    if (fill(&conns[c].in) < 0) {
        /* Let a newcomer's last words (a HELLO) be read first. */
        char line[NET_LINE_MAX];
        if (!conns[c].hello && take_line(&conns[c].in, line, sizeof line)) greet(c, line);
        drop(c);
        return;
    }
    if (c == waiting_conn) return;
    char line[NET_LINE_MAX];
    while (conns[c].open && take_line(&conns[c].in, line, sizeof line)) {
        if (!conns[c].hello) { greet(c, line); continue; }
        NetMsg m;
        if (net_parse(line, &m) == 0 && m.kind == NM_BYE) { drop(c); return; }
        /* Anything else outside a wait (a stray ACTION) is ignored. */
    }
}

/* See net.h: accept newcomers, read the waiting joiners, notice departures. */
int net_host_poll(int ms)
{
    if (role != NET_HOST) return 0;
    int before = nlobby + nevents;
    fd_set set;
    FD_ZERO(&set);
    int maxfd = listen_fd;
    FD_SET(listen_fd, &set);
    for (int c = 0; c < MAX_CONNS; c++) {
        if (!conns[c].open || c == waiting_conn) continue;
        FD_SET(conns[c].in.fd, &set);
        maxfd = MAX(maxfd, conns[c].in.fd);
    }
    struct timeval tv = { ms / 1000, (ms % 1000) * 1000 };
    if (select(maxfd + 1, &set, NULL, NULL, &tv) <= 0) return 0;
    if (FD_ISSET(listen_fd, &set)) {
        int fd = accept(listen_fd, NULL, NULL);
        if (fd >= 0) {
            int c;
            for (c = 0; c < MAX_CONNS && conns[c].open; c++) ;
            if (c == MAX_CONNS) {
                const char *full = "REFUSE " FULL_MESSAGE "\n";
                if (write(fd, full, strlen(full)) < 0) { /* nothing to do */ }
                close(fd);
            } else {
                set_nonblock(fd);
                memset(&conns[c], 0, sizeof conns[c]);
                conns[c].in.fd = fd;
                conns[c].open = 1;
                conns[c].player = -1;
            }
        }
    }
    for (int c = 0; c < MAX_CONNS; c++)
        if (conns[c].open && c != waiting_conn && FD_ISSET(conns[c].in.fd, &set)) service(c);
    return nlobby + nevents != before;
}

static const char *const cpu_name = "Lord Blueblood";

/* See net.h: host, joiners in order of arrival, then one computer player
 * only if a seat is free. */
void net_build_table(GameSettings *s, const char *hname, int cpu_difficulty)
{
    copy_str(host_name, sizeof host_name, hname);
    s->nplayers = 0;
    copy_str(s->names[s->nplayers], NAME_LEN, hname);
    s->cpu[s->nplayers++] = 0;
    for (int p = 0; p < MAXPLAYERS; p++) { seat_remote[p] = 0; seat_conn[p] = -1; }
    for (int i = 0; i < nlobby && s->nplayers < MAXPLAYERS; i++) {
        int p = s->nplayers++, c = lobby[i];
        copy_str(s->names[p], NAME_LEN, conns[c].name);
        s->cpu[p] = 0;
        seat_remote[p] = 1;
        seat_conn[p] = c;
        conns[c].player = p;
    }
    /* One computer player, only if a seat is free. */
    if (s->nplayers < MAXPLAYERS) {
        int p = s->nplayers++;
        copy_str(s->names[p], NAME_LEN, cpu_name);
        s->cpu[p] = 1;
        s->difficulty[p] = cpu_difficulty;
    }
    nlobby = 0;
    lobby_open = 0;
}

void net_set_host_name(const char *name) { copy_str(host_name, sizeof host_name, name); }

/* See net.h: START and PLAYER to every joiner, then the first STATE. */
void net_start_game(Game *g)
{
    hgame = g;
    last_n = -1;
    sent_log = MAX(0, g->log_count - LOG_LINES);
    for (int p = 0; p < g->nplayers; p++) {
        if (!net_seat_connected(p)) continue;
        int fd = conns[seat_conn[p]].in.fd;
        int st[2] = { p, g->nplayers };
        send_msg(fd, NM_START, st, 2, NULL);
        for (int q = 0; q < g->nplayers; q++) {
            int pl[3] = { q, g->pl[q].cpu, g->pl[q].difficulty };
            send_msg(fd, NM_PLAYER, pl, 3, g->pl[q].name);
        }
    }
    net_broadcast(g);
}

/* Send to a seat; a failed write means the joiner has gone. */
static void send_to(int p, NetMsgKind kind, const int *num, int nnum, const char *text)
{
    if (!net_seat_connected(p)) return;
    int c = seat_conn[p];
    if (send_msg(conns[c].in.fd, kind, num, nnum, text) < 0) drop(c);
}

/* See net.h: new LOG lines and the STATE, only when something changed. */
void net_broadcast(Game *g)
{
    if (role != NET_HOST || !hgame || g != hgame || in_broadcast) return;
    static int num[NET_MAX_FIELDS];
    int n = net_encode_state(g, num, NET_MAX_FIELDS);
    int first = MAX(sent_log, g->log_count - LOG_LINES);
    int last = g->log_count;
    /* Nothing new since last time: send nothing (the main loop calls this often). */
    if (first >= last && n == last_n && n > 0 && !memcmp(num, last_num, (size_t)n * sizeof num[0])) return;
    if (n > 0) { memcpy(last_num, num, (size_t)n * sizeof num[0]); last_n = n; }
    in_broadcast = 1;
    for (int p = 0; p < g->nplayers; p++) {
        if (!net_seat_connected(p)) continue;
        for (int i = first; i < last; i++) send_to(p, NM_LOG, NULL, 0, g->log[i % LOG_LINES]);
        if (n > 0) send_to(p, NM_STATE, num, n, NULL);
    }
    sent_log = last;
    in_broadcast = 0;
}

/* See net.h: a NOTE carrying "title|text". */
void net_note(int p, const char *title, const char *text)
{
    char buf[NET_LINE_MAX];
    snprintf(buf, sizeof buf, "%s|%s", title, text);
    send_to(p, NM_NOTE, NULL, 0, buf);
}

/* See net.h: TURN to every joiner. */
void net_turn(int p)
{
    if (!hgame) return;
    int num[1] = { p };
    for (int q = 0; q < hgame->nplayers; q++) send_to(q, NM_TURN, num, 1, NULL);
}

/* Wait for a line from remote seat p's connection, keeping everything else
 * alive (other joiners, latecomers, the host's window). 1 = got a line,
 * -1 = the seat dropped or the host gave up waiting. */
static int seat_line(int p, char *line, size_t size)
{
    for (;;) {
        if (!net_seat_connected(p)) return -1;
        int c = seat_conn[p];
        if (take_line(&conns[c].in, line, size)) return 1;
        waiting_conn = c;
        int ready = wait_readable(conns[c].in.fd, 100);
        if (ready && fill(&conns[c].in) < 0) {
            int got = take_line(&conns[c].in, line, size);
            waiting_conn = -1;
            if (got) { drop(c); return 1; }
            drop(c);
            return -1;
        }
        net_host_poll(0);
        waiting_conn = -1;
        if (idle()) {                   /* the host is leaving */
            net_close("The host has ended the game.");
            return -1;
        }
    }
}

/* See net.h: send ASK and wait for the matching ANSWER, keeping the host's
 * window and the other connections alive. */
int net_ask(int p, NetAsk kind, int a, int b, int c, const char *text)
{
    if (role != NET_HOST || !net_seat_connected(p)) return -1;
    net_broadcast(hgame);
    int id = ++ask_id;
    int num[5] = { id, kind, a, b, c };
    send_to(p, NM_ASK, num, 5, text ? text : "");
    char line[NET_LINE_MAX];
    while (seat_line(p, line, sizeof line) > 0) {
        NetMsg m;
        if (net_parse(line, &m) < 0) continue;
        if (m.kind == NM_ANSWER && m.num[0] == id) return m.num[1];
        if (m.kind == NM_BYE) { drop(seat_conn[p]); break; }
    }
    return -1;
}

/* See net.h: BYE to everyone, close; hosted seats of joiners go to computer
 * players. */
void net_close(const char *why)
{
    if (role == NET_HOST) {
        for (int c = 0; c < MAX_CONNS; c++) {
            if (!conns[c].open) continue;
            send_msg(conns[c].in.fd, NM_BYE, NULL, 0, why ? why : "The host has closed the game.");
            close_conn(c);
        }
        if (listen_fd >= 0) close(listen_fd);
        listen_fd = -1;
        if (hgame)
            for (int p = 0; p < hgame->nplayers; p++)
                if (seat_remote[p] && !hgame->pl[p].cpu) { hgame->pl[p].cpu = 1; hgame->pl[p].difficulty = DIFF_MEDIUM; }
        for (int p = 0; p < MAXPLAYERS; p++) seat_conn[p] = -1;
        nlobby = 0;
        lobby_open = 0;
    } else if (role == NET_CLIENT) {
        net_client_close(why);
    }
    role = NET_NONE;
}

/* See net.h: in a child of fork(), drop the parent's connections silently. */
void net_after_fork(void)
{
    for (int c = 0; c < MAX_CONNS; c++) if (conns[c].open) { close(conns[c].in.fd); conns[c].open = 0; }
    if (listen_fd >= 0) close(listen_fd);
    listen_fd = -1;
    if (cin.fd >= 0) close(cin.fd);
    cin.fd = -1;
    ssh_pid = -1;
    nlobby = 0;
    hgame = NULL;
    role = NET_NONE;
}

/* ==== the network seat's Decider ==== */

static void nd_deal_buy(Game *g, int p, const int *cards, int n, int *want)
{
    char text[NET_LINE_MAX] = "";
    int at = 0;
    for (int i = 0; i < n; i++) at += snprintf(text + at, sizeof text - (size_t)at, "%s%d", i ? " " : "", cards[i]);
    int r = net_ask(p, NA_DEAL_BUY, n, 0, 0, text);
    if (r < 0) { ai_decider.deal_buy(g, p, cards, n, want); return; }
    for (int i = 0; i < n; i++) want[i] = (r >> i) & 1;
}

#define ASK_OR_AI(kind, a, b, c, ai_call) \
    do { int r_ = net_ask(p, kind, a, b, c, NULL); return r_ >= 0 ? r_ : (ai_call); } while (0)

/* Rule 7 for a joiner (the computer answers if he is gone). */
static int nd_buy(Game *g, int p, int sp, int price, int hb)
{ ASK_OR_AI(NA_BUY, sp, price, hb, ai_decider.buy(g, p, sp, price, hb)); }

/* Rule 7's bid for a joiner. */
static int nd_max_bid(Game *g, int p, int sp, int min)
{ ASK_OR_AI(NA_MAX_BID, sp, min, 0, ai_decider.max_bid(g, p, sp, min)); }

/* Rule 6 for a joiner. */
static int nd_jail_pay(Game *g, int p)
{ ASK_OR_AI(NA_JAIL_PAY, 0, 0, 0, ai_decider.jail_pay(g, p)); }

/* Rule 17 for a joiner. */
static int nd_use_pass(Game *g, int p, int with, int without)
{ ASK_OR_AI(NA_USE_PASS, with, without, 0, ai_decider.use_pass(g, p, with, without)); }

/* Rule 2 for a joiner. */
static int nd_direction(Game *g, int p, int fwd, int back)
{ ASK_OR_AI(NA_DIRECTION, fwd, back, 0, ai_decider.direction(g, p, fwd, back)); }

/* Rule 8 for a joiner. */
static int nd_speculate(Game *g, int p)
{ ASK_OR_AI(NA_SPECULATE, 0, 0, 0, ai_decider.speculate(g, p)); }

/* Rule 13 for a joiner. */
static int nd_luxury(Game *g, int p)
{ ASK_OR_AI(NA_LUXURY, 0, 0, 0, ai_decider.luxury(g, p)); }

/* Single Tax Rule 4 for a joiner. */
static int nd_st_build(Game *g, int p, int sp)
{ ASK_OR_AI(NA_ST_BUILD, sp, 0, 0, ai_decider.st_build(g, p, sp)); }

/* The Single Tax vote for a joiner. */
static int nd_vote(Game *g, int p, int proposer)
{ ASK_OR_AI(NA_VOTE, proposer, 0, 0, ai_decider.vote_single_tax(g, p, proposer)); }

/* Rule 9's Trust for a joiner. */
static int nd_trust(Game *g, int p, int other)
{ ASK_OR_AI(NA_TRUST, other, 0, 0, ai_decider.join_trust(g, p, other)); }

/* The joiner picks a sale; the host carries it out. */
static int nd_raise(Game *g, int p, int need)
{
    int r = net_ask(p, NA_RAISE, need, 0, 0, NULL);
    if (r < 0) return ai_decider.raise(g, p, need);
    if (r == 0) return 0;
    int kind, sp, buyer;
    net_sale_decode(r, &kind, &sp, &buyer);
    if (sp < 0 || sp >= NSPACES) return 0;
    const char *err = "?";
    if (kind == 1) err = game_sell_house(g, p, sp);
    else if (kind == 3) err = game_sell_to_pack(g, p, sp);
    else if (kind == 2 && buyer >= 0 && buyer < g->nplayers && g->pl[buyer].cpu) {
        int offer = ai_offer_for(g, buyer, sp);
        err = offer > 0 ? game_trade(g, p, buyer, sp, offer) : game_sell_to_pack(g, p, sp);
    }
    if (err) net_note(p, "Raising Money", err);
    return err == NULL;
}

/* The joiner's own business on his turn: ACTIONs until he throws. */
static void nd_pre_roll(Game *g, int p)
{
    if (!net_seat_connected(p)) { ai_decider.pre_roll(g, p); return; }
    net_broadcast(g);
    net_turn(p);
    char line[NET_LINE_MAX];
    while (seat_line(p, line, sizeof line) > 0) {
        NetMsg m;
        if (net_parse(line, &m) < 0) continue;
        if (m.kind == NM_BYE) { drop(seat_conn[p]); break; }
        if (m.kind != NM_ACTION) continue;
        if (m.num[0] == NX_ROLL) return;
        if (action_fn && action_fn(g, p, &m)) return;
        if (!action_fn) net_note(p, "Not Available", "The host cannot do that.");
        net_broadcast(g);
        net_turn(p);
    }
    /* The joiner left during his turn: the computer finishes it. */
    if (g->pl[p].cpu) ai_decider.pre_roll(g, p);
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

/* ==== addresses ==== */

int net_parse_address(const char *in, char *target, size_t tsize, char *host, size_t hsize,
                      int *port, char *err, size_t errsize)
{
    char buf[256];
    copy_str(buf, sizeof buf, in);
    /* Trim blanks the player may have typed around it. */
    char *s = buf;
    while (*s == ' ' || *s == '\t') s++;
    for (char *e = s + strlen(s); e > s && (e[-1] == ' ' || e[-1] == '\t'); ) *--e = '\0';
    if (!*s) { snprintf(err, errsize, "Enter the host's IP address or host name."); return -1; }

    char user[128] = "";
    char *at = strrchr(s, '@');
    if (at) {
        *at = '\0';
        copy_str(user, sizeof user, s);
        s = at + 1;
        if (!user[0]) { snprintf(err, errsize, "There is nothing before the @ (the user name)."); return -1; }
    }
    char *portstr = NULL;
    if (*s == '[') {                            /* [IPv6]:port */
        char *close_b = strchr(s, ']');
        if (!close_b) { snprintf(err, errsize, "An IPv6 address in [ ] is missing its ]."); return -1; }
        *close_b = '\0';
        if (close_b[1] == ':') portstr = close_b + 2;
        else if (close_b[1]) { snprintf(err, errsize, "Only :port may follow the ] of an IPv6 address."); return -1; }
        s++;
    } else {
        char *first = strchr(s, ':'), *last = strrchr(s, ':');
        if (first && first == last) { *first = '\0'; portstr = first + 1; }   /* host:port */
        /* Two or more colons: a bare IPv6 address, which can carry no port. */
    }
    if (!*s) { snprintf(err, errsize, "The host's address is missing."); return -1; }
    for (const char *p = s; *p; p++) {
        if (*p == ' ' || *p == '/' || *p == '\\' || *p == ';' || *p == '\'' || *p == '"' || *p == '`' || *p == '$') {
            snprintf(err, errsize, "\"%s\" is not a host name or an IP address.", s);
            return -1;
        }
    }
    if (portstr) {
        char *end;
        long v = strtol(portstr, &end, 10);
        if (!*portstr || *end || v <= 0 || v > 65535) {
            snprintf(err, errsize, "\"%s\" is not a port number (1 to 65535).", portstr);
            return -1;
        }
        *port = (int)v;
    }
    copy_str(host, hsize, s);
    if (user[0]) snprintf(target, tsize, "%s@%s", user, s);
    else copy_str(target, tsize, s);
    return 0;
}

/* One address as text. */
static void addr_text(const struct sockaddr *sa, char *out, size_t size)
{
    out[0] = '\0';
    if (getnameinfo(sa, sa->sa_family == AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in),
                    out, (socklen_t)size, NULL, 0, NI_NUMERICHOST) != 0)
        out[0] = '\0';
}

int net_resolve(const char *host, char *found, size_t fsize, char *err, size_t errsize)
{
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    found[0] = '\0';
    /* An IP address first: no lookup is needed (or wanted) for those. */
    hints.ai_flags = AI_NUMERICHOST;
    int r = getaddrinfo(host, NULL, &hints, &res);
    if (r != 0) {
        hints.ai_flags = AI_ADDRCONFIG;
        r = getaddrinfo(host, NULL, &hints, &res);
    }
    if (r != 0) {
        if (r == EAI_NONAME || r == EAI_FAIL
#ifdef EAI_NODATA
            || r == EAI_NODATA
#endif
            )
            snprintf(err, errsize,
                     "The host name \"%s\" was not found. A name must be known to DNS or listed in this machine's "
                     "hosts file (/etc/hosts), for example:\n\n    192.168.1.20   %s\n\nOr type the host's IP "
                     "address instead.", host, host);
        else if (r == EAI_AGAIN)
            snprintf(err, errsize, "The host name \"%s\" could not be looked up just now (the DNS server did not "
                     "answer). Try again, use the IP address, or list the name in /etc/hosts.", host);
        else
            snprintf(err, errsize, "The host name \"%s\" could not be looked up: %s.", host, gai_strerror(r));
        return -1;
    }
    size_t at = 0;
    for (struct addrinfo *a = res; a; a = a->ai_next) {
        char one[INET6_ADDRSTRLEN + 8];
        addr_text(a->ai_addr, one, sizeof one);
        if (!one[0] || strstr(found, one)) continue;        /* (each address once) */
        int n = snprintf(found + at, fsize - at, "%s%s", at ? ", " : "", one);
        if (n < 0 || (size_t)n >= fsize - at) break;
        at += (size_t)n;
    }
    freeaddrinfo(res);
    return 0;
}

int net_local_addresses(char *out, size_t size)
{
    struct ifaddrs *list = NULL;
    out[0] = '\0';
    if (getifaddrs(&list) != 0) return 0;
    int count = 0;
    size_t at = 0;
    for (int pass = 0; pass < 2; pass++) {              /* IPv4, then IPv6 */
        int family = pass == 0 ? AF_INET : AF_INET6;
        for (struct ifaddrs *i = list; i; i = i->ifa_next) {
            if (!i->ifa_addr || i->ifa_addr->sa_family != family) continue;
            if (family == AF_INET) {
                const struct sockaddr_in *a = (const struct sockaddr_in *)i->ifa_addr;
                if ((ntohl(a->sin_addr.s_addr) >> 24) == 127) continue;         /* loopback */
            } else {
                const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)i->ifa_addr;
                if (IN6_IS_ADDR_LOOPBACK(&a->sin6_addr) || IN6_IS_ADDR_LINKLOCAL(&a->sin6_addr)) continue;
            }
            char one[INET6_ADDRSTRLEN + 8];
            addr_text(i->ifa_addr, one, sizeof one);
            if (!one[0] || strstr(out, one)) continue;
            int n = snprintf(out + at, size - at, "%s%s", at ? ", " : "", one);
            if (n < 0 || (size_t)n >= size - at) { pass = 2; break; }
            at += (size_t)n;
            count++;
        }
    }
    freeifaddrs(list);
    return count;
}

/* ==== joining ==== */


/* What ssh (or the relay) printed on its standard error, tidied. */
static void ssh_errors(char *out, size_t size)
{
    out[0] = '\0';
    if (!ssh_err) return;
    fflush(ssh_err);
    rewind(ssh_err);
    size_t n = fread(out, 1, size - 1, ssh_err);
    out[n] = '\0';
    while (n && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) out[--n] = '\0';
    for (char *p = out; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
}

/* The joiner's side of net_close: BYE, close the pipe, let ssh finish. */
void net_client_close(const char *why)
{
    if (cin.fd >= 0) {
        if (why) send_msg(cin.fd, NM_BYE, NULL, 0, why);
        close(cin.fd);
    }
    cin.fd = -1;
    cin.len = 0;
    if (ssh_pid > 0) {
        int st;
        for (int i = 0; i < 20 && waitpid(ssh_pid, &st, WNOHANG) == 0; i++) usleep(50000);
        if (waitpid(ssh_pid, &st, WNOHANG) == 0) { kill(ssh_pid, SIGTERM); waitpid(ssh_pid, &st, 0); }
    }
    ssh_pid = -1;
    if (ssh_err) fclose(ssh_err);
    ssh_err = NULL;
}

/* See net.h: start ssh with the relay command, send HELLO, wait for WELCOME
 * or REFUSE. */
int net_join(const char *host, int port, const char *remote_cmd, const char *name, char *err, size_t errsize)
{
    net_close(NULL);
    signal(SIGPIPE, SIG_IGN);
    /* The name must resolve (DNS or the hosts file) before ssh is tried, so
     * the player gets a plain answer rather than ssh's. */
    {
        char target[256], bare[256], found[512];
        int dummy_port = port;
        if (net_parse_address(host, target, sizeof target, bare, sizeof bare, &dummy_port, err, errsize) < 0)
            return -1;
        if (net_resolve(bare, found, sizeof found, err, errsize) < 0) return -1;
    }
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        snprintf(err, errsize, "Cannot make a pipe for ssh: %s.", strerror(errno));
        return -1;
    }
    ssh_err = tmpfile();
    const char *ssh = getenv("VIOLA_LANDLORD_SSH");
    if (!ssh || !*ssh) ssh = "ssh";
    char portbuf[16];
    snprintf(portbuf, sizeof portbuf, "%d", port);
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(err, errsize, "Cannot start ssh: %s.", strerror(errno));
        close(sv[0]); close(sv[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(sv[1], 0);
        dup2(sv[1], 1);
        if (ssh_err) dup2(fileno(ssh_err), 2);
        close(sv[0]);
        close(sv[1]);
        /* -T: no terminal; BatchMode: never ask for a password (keys only);
         * the remote command runs the relay on the host. */
        execlp(ssh, ssh, "-T", "-o", "BatchMode=yes", "-o", "ConnectTimeout=15", host,
               remote_cmd && *remote_cmd ? remote_cmd : "viola-landlord", "--relay", portbuf, (char *)NULL);
        fprintf(stderr, "cannot run %s: %s\n", ssh, strerror(errno));
        _exit(127);
    }
    close(sv[1]);
    ssh_pid = pid;
    cin.fd = sv[0];
    cin.len = 0;
    set_nonblock(cin.fd);
    role = NET_CLIENT;

    int hello[1] = { NET_PROTOCOL };
    send_msg(cin.fd, NM_HELLO, hello, 1, name);
    long until = now_ms() + 30000;
    while (now_ms() < until) {
        NetMsg m;
        int r = net_client_recv(&m, 100);
        if (r < 0) break;
        if (r == 0) { if (idle()) break; continue; }
        if (m.kind == NM_WELCOME) return 0;
        if (m.kind == NM_REFUSE) {
            snprintf(err, errsize, "%s", m.text);
            net_close(NULL);
            return -1;
        }
    }
    char why[600];
    usleep(200000);
    ssh_errors(why, sizeof why);
    if (why[0]) snprintf(err, errsize, "Could not join the game on %s: %s", host, why);
    else snprintf(err, errsize, "Could not join the game on %s: no answer from it. Is it reachable, and is a "
                  "game being hosted there?", host);
    net_close(NULL);
    return -1;
}

/* See net.h: the next complete message from the host. */
int net_client_recv(NetMsg *m, int ms)
{
    if (role != NET_CLIENT || cin.fd < 0) return -1;
    char line[NET_LINE_MAX];
    for (;;) {
        while (take_line(&cin, line, sizeof line))
            if (net_parse(line, m) == 0) return 1;
        if (ms <= 0) {
            if (fill(&cin) < 0) { if (take_line(&cin, line, sizeof line) && net_parse(line, m) == 0) return 1; return -1; }
            if (memchr(cin.buf, '\n', (size_t)cin.len)) continue;
            return 0;
        }
        if (!wait_readable(cin.fd, ms)) return 0;
        if (fill(&cin) < 0) {
            if (take_line(&cin, line, sizeof line) && net_parse(line, m) == 0) return 1;
            return -1;
        }
        ms = 0;
    }
}

/* See net.h: one message to the host. */
int net_client_send(NetMsgKind kind, const int *num, int nnum, const char *text)
{
    if (role != NET_CLIENT || cin.fd < 0) return -1;
    return send_msg(cin.fd, kind, num, nnum, text);
}

/* ==== the relay ==== */

int net_relay(int port)
{
    signal(SIGPIPE, SIG_IGN);
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (fd < 0 || connect(fd, (struct sockaddr *)&a, sizeof a) < 0) {
        fprintf(stderr, "no game is being hosted on this machine (nothing is listening on port %d). "
                        "Ask the host to choose Game > Host a Network Game first.\n", port);
        return 1;
    }
    char buf[8192];
    int in_open = 1;
    for (;;) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        if (in_open) FD_SET(0, &set);
        if (select(fd + 1, &set, NULL, NULL, NULL) < 0) { if (errno == EINTR) continue; break; }
        if (in_open && FD_ISSET(0, &set)) {
            ssize_t r = read(0, buf, sizeof buf);
            if (r <= 0) { in_open = 0; shutdown(fd, SHUT_WR); }
            else if (write(fd, buf, (size_t)r) != r) break;
        }
        if (FD_ISSET(fd, &set)) {
            ssize_t r = read(fd, buf, sizeof buf);
            if (r <= 0) break;
            for (ssize_t at = 0; at < r; ) {
                ssize_t w = write(1, buf + at, (size_t)(r - at));
                if (w <= 0) { close(fd); return 0; }
                at += w;
            }
        }
    }
    close(fd);
    return 0;
}
