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

/* tests/net.c - the parts of network play that exist so far:
 *   - every kind of message survives net_format then net_parse;
 *   - malformed lines are refused;
 *   - a game whose seats all use net_decider (none connected) plays to the
 *     end, the computer answering for the absent players;
 *   - the connection stubs fail politely. */
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "net.h"

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void round_trips(void)
{
    char line[NET_LINE_MAX];
    for (int k = 0; k < NM_COUNT; k++) {
        int nf = net_fields_of((NetMsgKind)k), num[8];
        for (int i = 0; i < nf; i++) num[i] = (i + 1) * (k % 2 ? -7 : 11);
        int n = net_format(line, sizeof line, (NetMsgKind)k, num, nf, "Fifth Avenue\nfor $100");
        CHECK(n > 0, "format %s", net_kind_name((NetMsgKind)k));
        NetMsg m;
        CHECK(net_parse(line, nf, &m) == 0, "parse %s: [%s]", net_kind_name((NetMsgKind)k), line);
        CHECK(m.kind == k, "kind %s", net_kind_name((NetMsgKind)k));
        for (int i = 0; i < nf; i++) CHECK(m.num[i] == num[i], "%s field %d", net_kind_name((NetMsgKind)k), i);
        CHECK(!strcmp(m.text, "Fifth Avenue for $100"), "%s text [%s]", net_kind_name((NetMsgKind)k), m.text);
    }
    NetMsg m;
    CHECK(net_parse("NONSENSE 1 2", 2, &m) < 0, "unknown kind accepted");
    CHECK(net_parse("ANSWER 1", 2, &m) < 0, "missing field accepted");
    CHECK(net_parse("ANSWER 1 x", 2, &m) < 0, "non-number accepted");
    CHECK(net_parse("ANSWER 3 -1\r\n", 2, &m) == 0 && m.num[0] == 3 && m.num[1] == -1, "ANSWER with CRLF");
    char small[8];
    CHECK(net_format(small, sizeof small, NM_CHAT, NULL, 0, "far too long for this") < 0, "overflow not caught");
}

static void absent_players(void)
{
    static Game g;
    GameSettings s;
    memset(&s, 0, sizeof s);
    s.nplayers = 3;
    s.end_wages = 3;
    s.seed = 42;
    for (int p = 0; p < 3; p++) { s.difficulty[p] = p % 3; snprintf(s.names[p], NAME_LEN, "Remote %d", p); }
    game_new(&g, &s);
    for (int p = 0; p < 3; p++) game_set_decider(&g, p, &net_decider);
    game_deal(&g);
    for (int t = 0; t < 3000 && !g.over; t++) game_turn(&g);
    CHECK(g.over, "a game of absent network players did not end");
}

int main(void)
{
    round_trips();
    absent_players();
    char err[400];
    CHECK(net_host(NET_PORT, err, sizeof err) < 0 && err[0], "net_host stub");
    CHECK(net_join("localhost", NET_PORT, "Lee", err, sizeof err) < 0 && err[0], "net_join stub");
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    return 0;
}
