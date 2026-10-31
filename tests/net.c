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

/* tests/net.c - network play, end to end, without a real ssh:
 *
 *   This program also plays the part of ssh: VIOLA_LANDLORD_SSH points at
 *   it, and when it is started with ssh's arguments it runs the relay, as
 *   sshd would on the host. So every byte goes the real way:
 *   joiner -> "ssh" -> relay -> host's localhost socket.
 *
 *   1. the message format and the STATE encoding round-trip;
 *   2. three joiners take seats; a fourth (the fifth human) is turned away
 *      with the "table is full" warning;
 *   3. with four humans the table has no computer player, and a whole game is
 *      played over the network (the joiners are little robots answering
 *      their questions and throwing on their turns);
 *   4. with two humans there is exactly one computer player; a joiner who
 *      disconnects mid-game is replaced by a computer player and the game
 *      still ends;
 *   5. joining a host with no game running fails with the relay's message. */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ai.h"
#include "game.h"
#include "net.h"

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)

static char self[4096];         /* this program, to run as "ssh" and as the relay */
static int port;

/* ---- 1. the format ---- */

static void formats(void)
{
    char line[NET_LINE_MAX];
    for (int k = 0; k < NM_COUNT; k++) {
        int nf = net_fields_of((NetMsgKind)k), num[8];
        if (nf < 0) continue;
        for (int i = 0; i < nf; i++) num[i] = (i + 1) * (k % 2 ? -7 : 11);
        CHECK(net_format(line, sizeof line, (NetMsgKind)k, num, nf, "Fifth Avenue\nfor $100") > 0, "format %d", k);
        NetMsg m;
        CHECK(net_parse(line, &m) == 0 && m.kind == k, "parse %s", line);
        for (int i = 0; i < nf; i++) CHECK(m.num[i] == num[i], "%s field %d", net_kind_name((NetMsgKind)k), i);
        CHECK(!strcmp(m.text, "Fifth Avenue for $100"), "%s text [%s]", net_kind_name((NetMsgKind)k), m.text);
    }
    NetMsg m;
    CHECK(net_parse("NONSENSE 1 2", &m) < 0, "unknown kind accepted");
    CHECK(net_parse("ANSWER 1", &m) < 0, "missing field accepted");
    CHECK(net_parse("ANSWER 1 x", &m) < 0, "non-number accepted");
    CHECK(net_parse("ANSWER 3 -1\r\n", &m) == 0 && m.num[0] == 3 && m.num[1] == -1, "ANSWER with CRLF");

    /* STATE: a game in progress survives the trip. */
    static Game a, b;
    GameSettings s;
    memset(&s, 0, sizeof s);
    s.nplayers = 3;
    s.seed = 7;
    for (int p = 0; p < 3; p++) { s.cpu[p] = 1; s.difficulty[p] = p; }
    game_new(&a, &s);
    for (int p = 0; p < 3; p++) game_set_decider(&a, p, &ai_decider);
    game_deal(&a);
    for (int t = 0; t < 40; t++) game_turn(&a);
    static int num[NET_MAX_FIELDS];
    int n = net_encode_state(&a, num, NET_MAX_FIELDS);
    CHECK(n > 0, "encode");
    CHECK(net_format(line, sizeof line, NM_STATE, num, n, NULL) > 0, "STATE fits a line");
    CHECK(net_parse(line, &m) == 0 && m.nnum == n, "STATE parses (%d of %d)", m.nnum, n);
    CHECK(net_decode_state(&b, m.num, m.nnum) == 0, "decode");
    CHECK(b.cur == a.cur && b.treasury == a.treasury && b.turn_no == a.turn_no, "STATE header");
    for (int p = 0; p < 3; p++) CHECK(b.pl[p].cash == a.pl[p].cash && b.pl[p].pos == a.pl[p].pos, "player %d", p);
    for (int sp = 0; sp < NSPACES; sp++) CHECK(b.owner[sp] == a.owner[sp] && b.houses[sp] == a.houses[sp], "space %d", sp);
    int kind, sp, buyer;
    net_sale_decode(net_sale_encode(2, 38, 3), &kind, &sp, &buyer);
    CHECK(kind == 2 && sp == 38 && buyer == 3, "sale encoding");
}

/* ---- a robot joiner ---- */

static int robot_idle(void) { return 0; }

/* Join as `name`, answer every question, throw on every turn; leave after
 * `leave_after` of its own turns (0: never). Exit status: 0 played to the
 * end, 2 could not join, 3 turned away with the full-table warning. */
static void robot(const char *name, int leave_after)
{
    char err[1024];
    net_set_idle(robot_idle);
    if (net_join("testhost", port, self, name, err, sizeof err) < 0)
        _exit(strstr(err, "table is full") ? 3 : 2);
    int me = -1, turns = 0;
    NetMsg m;
    for (;;) {
        int r = net_client_recv(&m, 2000);
        if (r < 0) _exit(0);
        if (r == 0) continue;
        switch (m.kind) {
        case NM_START: me = m.num[0]; break;
        case NM_BYE:   _exit(0);
        case NM_ASK: {
            int ans = 0;
            switch (m.num[1]) {
            case NA_DEAL_BUY: ans = 3; break;                   /* the first two cards */
            case NA_BUY: case NA_SPECULATE: case NA_LUXURY: ans = 1; break;
            default: ans = 0; break;
            }
            int a[2] = { m.num[0], ans };
            net_client_send(NM_ANSWER, a, 2, NULL);
            break;
        }
        case NM_TURN:
            if (m.num[0] != me) break;
            if (leave_after && ++turns >= leave_after) { net_close(NULL); _exit(0); }
            {
                int a[4] = { NX_ROLL, 0, 0, 0 };
                net_client_send(NM_ACTION, a, 4, NULL);
            }
            break;
        default: break;
        }
    }
}

static pid_t spawn_robot(const char *name, int leave_after)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) { net_after_fork(); robot(name, leave_after); }
    return pid;
}

/* Keep the host answering until `n` joiners wait in the lobby. */
static int wait_lobby(int n)
{
    for (int i = 0; i < 400 && net_lobby_count() < n; i++) net_host_poll(50);
    return net_lobby_count() == n;
}

/* Keep the host answering until child `pid` exits; its exit status. */
static int wait_child(pid_t pid)
{
    for (int i = 0; i < 400; i++) {
        int st;
        if (waitpid(pid, &st, WNOHANG) == pid) {
            if (!WIFEXITED(st)) printf("(child %d: signal %d)\n", (int)pid, WIFSIGNALED(st) ? WTERMSIG(st) : -1);
            return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
        }
        if (!net_host_poll(50)) usleep(20000);
    }
    kill(pid, SIGKILL);
    return -1;
}

static void broadcast(Game *g) { net_broadcast(g); }

/* Seat the table, play the game to its end; returns the number of
 * computer players seated at the start. */
static int play_hosted(Game *g, int *nplayers, int *over)
{
    GameSettings s;
    memset(&s, 0, sizeof s);
    s.end_wages = 3;
    s.seed = 99;
    net_build_table(&s, "Host", DIFF_MEDIUM);
    int ncpu = 0;
    for (int p = 0; p < s.nplayers; p++) ncpu += s.cpu[p];
    game_new(g, &s);
    for (int p = 0; p < g->nplayers; p++)
        game_set_decider(g, p, p == 0 || g->pl[p].cpu ? &ai_decider : &net_decider);
    g->hooks.changed = broadcast;
    net_start_game(g);
    game_deal(g);
    for (int t = 0; t < 4000 && !g->over; t++) game_turn(g);
    net_broadcast(g);
    *nplayers = g->nplayers;
    *over = g->over;
    return ncpu;
}

int main(int argc, char **argv)
{
    /* Started as "ssh -T -o ... host COMMAND --relay PORT": be sshd + relay. */
    if (argc >= 3 && !strcmp(argv[argc - 2], "--relay")) return net_relay(atoi(argv[argc - 1]));

    if (!realpath(argv[0], self)) { perror("realpath"); return 1; }
    setenv("VIOLA_LANDLORD_SSH", self, 1);
    port = 20000 + (int)(getpid() % 20000);

    formats();

    /* 5. nobody hosting: the relay's own message comes back. */
    {
        char err[1024];
        net_set_idle(robot_idle);
        CHECK(net_join("testhost", port, self, "Early", err, sizeof err) < 0 && strstr(err, "no game is being hosted"),
              "joining nobody: [%s]", err);
    }

    static Game g;
    char err[400];
    /* 2 + 3: four humans. */
    CHECK(net_host(port, err, sizeof err) == 0, "host: %s", err);
    pid_t bots[3];
    bots[0] = spawn_robot("Ann", 0);
    bots[1] = spawn_robot("Bea", 0);
    bots[2] = spawn_robot("Cal", 0);
    CHECK(wait_lobby(3), "three joiners in the lobby (%d)", net_lobby_count());
    pid_t extra = spawn_robot("Dan", 0);
    CHECK(wait_child(extra) == 3, "the fifth human was not turned away with the table-full warning");
    int found_warning = 0;
    for (const char *e; (e = net_host_event()); ) if (strstr(e, "Dan was turned away")) found_warning = 1;
    CHECK(found_warning, "the host was not told Dan was turned away");
    int np, over;
    int ncpu = play_hosted(&g, &np, &over);
    CHECK(np == 4 && ncpu == 0, "four humans: %d players, %d computer", np, ncpu);
    CHECK(over, "the four-human network game did not end");
    net_close("Game over.");
    for (int i = 0; i < 3; i++) { int st = wait_child(bots[i]); CHECK(st == 0, "robot %d exited %d", i, st); }

    /* 4. two humans, one of whom leaves. */
    CHECK(net_host(port + 1, err, sizeof err) == 0, "host again: %s", err);
    port = port + 1;
    pid_t quitter = spawn_robot("Eve", 4);
    CHECK(wait_lobby(1), "one joiner");
    ncpu = play_hosted(&g, &np, &over);
    CHECK(np == 3 && ncpu == 1, "two humans: %d players, %d computer (want 3, 1)", np, ncpu);
    CHECK(over, "the game with a quitter did not end");
    CHECK(g.pl[1].cpu, "the quitter's seat was not taken by a computer player");
    net_close("Game over.");
    { int st = wait_child(quitter); CHECK(st == 0, "quitter exited %d", st); }

    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    return 0;
}
