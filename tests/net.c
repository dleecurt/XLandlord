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

/* tests/net.c - network play, end to end, without a real ssh:
 *
 *   This program also plays the part of ssh: XLANDLORD_SSH points at
 *   it, and when it is started with ssh's arguments it runs the relay, as
 *   sshd would on the host. So every byte goes the real way:
 *   joiner -> "ssh" -> relay -> host's localhost socket.
 *
 *   1. the message format and the STATE encoding round-trip; addresses:
 *      IP addresses (v4 and v6), names, user@ and :port are understood, names
 *      are looked up in the hosts file / DNS, unknown names refused;
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
#include "util.h"

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

/* ---- addresses: IP addresses, names, ports, users ---- */

static void addresses(void)
{
    static const struct { const char *in, *target, *host; int port; } ok[] = {
        { "192.168.1.20",             "192.168.1.20",      "192.168.1.20", 1904 },
        { "lee@192.168.1.20:1905",    "lee@192.168.1.20",  "192.168.1.20", 1905 },
        { "  gamebox  ",              "gamebox",           "gamebox",      1904 },
        { "lee@gamebox:2000",         "lee@gamebox",       "gamebox",      2000 },
        { "fe80::1",                  "fe80::1",           "fe80::1",      1904 },
        { "[2001:db8::5]:1905",       "2001:db8::5",       "2001:db8::5",  1905 },
        { "ann@[2001:db8::5]",        "ann@2001:db8::5",   "2001:db8::5",  1904 },
    };
    for (int i = 0; i < (int)(sizeof ok / sizeof ok[0]); i++) {
        char target[256], host[256], err[300];
        int p = 1904;
        int r = net_parse_address(ok[i].in, target, sizeof target, host, sizeof host, &p, err, sizeof err);
        CHECK(r == 0 && !strcmp(target, ok[i].target) && !strcmp(host, ok[i].host) && p == ok[i].port,
              "address [%s] -> [%s] [%s] %d (%s)", ok[i].in, target, host, p, r ? err : "");
    }
    static const char *const bad[] = { "", "@host", "host:99999", "host:x", "[::1", "[::1]x", "a b", "host;rm" };
    for (int i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        char target[256], host[256], err[300] = "";
        int p = 1904;
        CHECK(net_parse_address(bad[i], target, sizeof target, host, sizeof host, &p, err, sizeof err) < 0 && err[0],
              "bad address [%s] accepted", bad[i]);
    }
    /* Labels: a name when one is known, else the address. */
    char label[128];
    net_host_label("127.0.0.1", label, sizeof label);
    CHECK(!strcmp(label, "localhost"), "label of 127.0.0.1: [%s]", label);
    net_host_label("192.0.2.77", label, sizeof label);
    CHECK(!strcmp(label, "192.0.2.77"), "label of an address with no name: [%s]", label);
    /* Lookups: an IP address as it is; a name from the hosts file; an
     * unknown name refused with advice about DNS and /etc/hosts. */
    char found[512], err[600];
    CHECK(net_resolve("127.0.0.1", found, sizeof found, err, sizeof err) == 0 && strstr(found, "127.0.0.1"),
          "resolve 127.0.0.1: %s", found);
    CHECK(net_resolve("::1", found, sizeof found, err, sizeof err) == 0 && strstr(found, "::1"), "resolve ::1");
    CHECK(net_resolve("localhost", found, sizeof found, err, sizeof err) == 0 && found[0],
          "resolve localhost (from /etc/hosts): %s", err);
    CHECK(net_resolve("no-such-host.invalid", found, sizeof found, err, sizeof err) < 0 && strstr(err, "/etc/hosts"),
          "an unknown name was not refused with advice: [%s]", err);
    char err2[600];
    CHECK(net_join("no-such-host.invalid", NET_PORT, self, "Nobody", err2, sizeof err2) < 0 && strstr(err2, "not found"),
          "joining an unknown name: [%s]", err2);
    char mine[512];
    net_local_addresses(mine, sizeof mine);
    CHECK(!strstr(mine, "127.0.0.1"), "loopback listed among this machine's addresses: %s", mine);
}

/* ---- a robot joiner ---- */

static int robot_idle(void) { return 0; }

/* What a robot does after `leave_after` of its own turns. */
enum { R_QUIT, R_DROP_BACK, R_DROP_GONE, R_STALL };
static int robot_mode = R_QUIT;

/* Join as `name`, answer every question, throw on every turn. After
 * `leave_after` of its own turns (0: never) it quits with a BYE (R_QUIT),
 * loses its connection and comes back two seconds later (R_DROP_BACK), loses
 * it for good (R_DROP_GONE), or stops answering (R_STALL). Exit status: 0
 * played to the end, 2 could not join, 3 turned away with the full-table
 * warning, 4 taken out for not answering, 5 could not rejoin, 6 turned away
 * because someone at the table already has that name. */
static void robot(const char *name, int leave_after)
{
    char err[1024];
    net_set_idle(robot_idle);
    /* Robots join alternately by name (found in /etc/hosts) and by IP address. */
    const char *where = name[0] % 2 ? "localhost" : "127.0.0.1";
    if (net_join(where, port, self, name, err, sizeof err) < 0)
        _exit(strstr(err, "table is full") ? 3 : strstr(err, "already called") ? 6 : 2);
    int me = -1, turns = 0;
    NetMsg m;
    for (;;) {
        int r = net_client_recv(&m, 2000);
        if (r < 0) _exit(0);
        if (r == 0) continue;
        switch (m.kind) {
        case NM_START: me = m.num[0]; break;
        case NM_BYE:   _exit(strstr(m.text, "did not answer") ? 4 : 0);
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
            if (leave_after && ++turns == leave_after) {
                if (robot_mode == R_QUIT) { net_close("A player has left."); _exit(0); }
                if (robot_mode == R_DROP_GONE) { net_close(NULL); _exit(0); }
                if (robot_mode == R_STALL) {        /* never answers again; waits for the BYE */
                    for (;;) {
                        int r2 = net_client_recv(&m, 2000);
                        if (r2 < 0) _exit(0);
                        if (r2 > 0 && m.kind == NM_BYE) _exit(strstr(m.text, "did not answer") ? 4 : 0);
                    }
                }
                /* R_DROP_BACK: the line goes dead (no BYE), then the same
                 * player comes back from the same machine. */
                net_close(NULL);
                sleep(2);
                if (net_join(where, port, self, name, err, sizeof err) < 0) _exit(5);
                break;          /* START, PLAYERs, STATE and the question again follow */
            }
            {
                int a[4] = { NX_ROLL, 0, 0, 0 };
                net_client_send(NM_ACTION, a, 4, NULL);
            }
            break;
        default: break;
        }
    }
}

/* Fork a robot joiner. */
/* `from` plays sshd's part: the client address the relay will report
 * (NULL: as if the relay were not started by sshd). */
static pid_t spawn_robot_from(const char *name, int leave_after, const char *from)
{
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        net_after_fork();
        if (from) {
            char sc[200];
            snprintf(sc, sizeof sc, "%s 50000 127.0.0.1 22", from);
            setenv("SSH_CONNECTION", sc, 1);
        } else {
            unsetenv("SSH_CONNECTION");
        }
        robot(name, leave_after);
    }
    return pid;
}

static pid_t spawn_robot(const char *name, int leave_after)
{
    return spawn_robot_from(name, leave_after, "127.0.0.1");
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

/* The tests in order; also the stand-in for ssh (see the top of the file). */
int main(int argc, char **argv)
{
    /* Started as "ssh -T -o ... host COMMAND --relay PORT": be sshd + relay. */
    if (argc >= 3 && !strcmp(argv[argc - 2], "--relay")) return net_relay(atoi(argv[argc - 1]));

    if (!realpath(argv[0], self)) { perror("realpath"); return 1; }
    setenv("XLANDLORD_SSH", self, 1);
    port = 20000 + (int)(getpid() % 20000);

    formats();
    addresses();

    /* 5. nobody hosting: the relay's own message comes back. */
    {
        char err[1024];
        net_set_idle(robot_idle);
        CHECK(net_join("127.0.0.1", port, self, "Early", err, sizeof err) < 0 && strstr(err, "no game is being hosted"),
              "joining nobody: [%s]", err);
    }

    static Game g;
    char err[400];
    /* 2 + 3: four humans. */
    CHECK(net_host(port, err, sizeof err) == 0, "host: %s", err);
    pid_t bots[3];
    /* Where they come from: Ann from 127.0.0.1 (named "localhost" in
     * /etc/hosts), Bea from a TEST-NET address that has no name (so its
     * address is shown), Cal not through sshd at all. */
    bots[0] = spawn_robot_from("Ann", 0, "127.0.0.1");
    CHECK(wait_lobby(1), "Ann in the lobby");
    bots[1] = spawn_robot_from("Bea", 0, "192.0.2.77");
    CHECK(wait_lobby(2), "Bea in the lobby");
    bots[2] = spawn_robot_from("Cal", 0, NULL);
    CHECK(wait_lobby(3), "three joiners in the lobby (%d)", net_lobby_count());
    CHECK(!strcmp(net_lobby_from(0), "localhost"), "Ann's machine: [%s]", net_lobby_from(0));
    CHECK(!strcmp(net_lobby_from(1), "192.0.2.77"), "Bea's machine: [%s]", net_lobby_from(1));
    CHECK(strstr(net_lobby_from(2), "unknown") != NULL, "Cal's machine: [%s]", net_lobby_from(2));
    pid_t extra = spawn_robot("Dan", 0);
    CHECK(wait_child(extra) == 3, "the fifth human was not turned away with the table-full warning");
    int found_warning = 0;
    for (const char *e; (e = net_host_event()); ) if (strstr(e, "Dan was turned away")) found_warning = 1;
    CHECK(found_warning, "the host was not told Dan was turned away");
    int np, over;
    int ncpu = play_hosted(&g, &np, &over);
    CHECK(np == 4 && ncpu == 0, "four humans: %d players, %d computer", np, ncpu);
    char here[256] = "";
    gethostname(here, sizeof here - 1);
    CHECK(!strcmp(net_player_from(0), here), "the host's seat shows this machine: [%s]", net_player_from(0));
    CHECK(!strcmp(net_player_from(1), "localhost") && !strcmp(net_player_from(2), "192.0.2.77"),
          "seats show the joiners' machines: [%s] [%s]", net_player_from(1), net_player_from(2));
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
    /* Nobody takes a leaver's seat: he is out, holding nothing, and the one
     * computer player is still the only one. */
    CHECK(g.pl[1].left && !g.pl[1].cpu, "the quitter did not leave the game (left %d, cpu %d)", g.pl[1].left, g.pl[1].cpu);
    int held = 0, cpus = 0;
    for (int sp = 0; sp < NSPACES; sp++) held += g.owner[sp] == 1 || (g.houses[sp] && g.house_owner[sp] == 1);
    for (int p = 0; p < g.nplayers; p++) cpus += g.pl[p].cpu && !g.pl[p].left;
    CHECK(held == 0 && g.pl[1].cash == 0, "the quitter still holds %d card(s) and $%d", held, g.pl[1].cash);
    CHECK(cpus == 1, "%d computer players at the end (want exactly 1)", cpus);
    net_close("Game over.");
    { int st = wait_child(quitter); CHECK(st == 0, "quitter exited %d", st); }

    /* 6. a dropped connection, back in time: the same seat, the game goes on. */
    net_set_timers(20, 60);
    port++;
    CHECK(net_host(port, err, sizeof err) == 0, "host (reconnect): %s", err);
    robot_mode = R_DROP_BACK;
    pid_t back = spawn_robot("Fay", 3);
    CHECK(wait_lobby(1), "Fay in the lobby");
    play_hosted(&g, &np, &over);
    CHECK(over, "the game with a reconnect did not end");
    CHECK(!g.pl[1].left, "Fay came back but was taken out of the game");
    int saw_back = 0;
    for (int i = MAX(0, g.log_count - LOG_LINES); i < g.log_count; i++)
        if (strstr(g.log[i % LOG_LINES], "is back at the table")) saw_back = 1;
    CHECK(saw_back, "the record does not say Fay came back");
    net_close("Game over.");
    { int st = wait_child(back); CHECK(st == 0, "reconnecting robot exited %d", st); }

    /* 7. a dropped connection, not back in time: he leaves the game. */
    net_set_timers(3, 60);
    port++;
    CHECK(net_host(port, err, sizeof err) == 0, "host (gone): %s", err);
    robot_mode = R_DROP_GONE;
    pid_t gone = spawn_robot("Gus", 3);
    CHECK(wait_lobby(1), "Gus in the lobby");
    play_hosted(&g, &np, &over);
    CHECK(over, "the game after a lost player did not end");
    CHECK(g.pl[1].left, "Gus never came back but is still at the table");
    net_close("Game over.");
    { int st = wait_child(gone); CHECK(st == 0, "vanishing robot exited %d", st); }

    /* 8. a player who stops answering: warned (on his side), then taken out. */
    net_set_timers(30, 2);
    net_set_warning(1, 1);
    port++;
    CHECK(net_host(port, err, sizeof err) == 0, "host (stall): %s", err);
    robot_mode = R_STALL;
    pid_t stall = spawn_robot("Hal", 3);
    CHECK(wait_lobby(1), "Hal in the lobby");
    play_hosted(&g, &np, &over);
    CHECK(over, "the game with a silent player did not end");
    CHECK(g.pl[1].left, "Hal stopped answering but was never taken out");
    net_close("Game over.");
    { int st = wait_child(stall); CHECK(st == 4, "the silent robot was not told it did not answer (exit %d)", st); }
    robot_mode = R_QUIT;
    net_set_warning(NET_WARN_SECS, NET_GRACE_SECS);
    net_set_timers(120, 120);

    /* 9. names are identities at the table: no two alike (in any case), and
     * not the host's. */
    port++;
    CHECK(net_host(port, err, sizeof err) == 0, "host (names): %s", err);
    net_set_host_name("Host");
    pid_t ivy = spawn_robot("Ivy", 0);
    CHECK(wait_lobby(1), "Ivy in the lobby");
    pid_t ivy2 = spawn_robot("IVY", 0);
    CHECK(wait_child(ivy2) == 6, "a second Ivy was not turned away for her name");
    pid_t host2 = spawn_robot("host", 0);
    CHECK(wait_child(host2) == 6, "a joiner called like the host was not turned away");
    CHECK(net_lobby_count() == 1, "the lobby should still hold only Ivy (%d)", net_lobby_count());
    net_close("Game over.");
    { int st = wait_child(ivy); CHECK(st == 0, "Ivy exited %d", st); }

    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    return 0;
}
