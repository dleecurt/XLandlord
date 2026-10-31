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

/* tests/engine.c - plays many games between computer players, with no
 * display, and checks that the rules engine behaves:
 *   - every game ends, in a sensible number of turns;
 *   - no player's cash ever goes below zero;
 *   - cards, houses and owners stay consistent;
 *   - the Single Tax and the advanced rules run without trouble;
 *   - HARD beats EASY far more often than not, and MEDIUM beats EASY.
 * Run by `make check`. With -v it prints the win counts. */
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "ai.h"

static int verbose;
static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void consistent(const Game *g)
{
    for (int p = 0; p < g->nplayers; p++) {
        CHECK(g->pl[p].cash >= 0, "player %d has negative cash %d", p, g->pl[p].cash);
        CHECK(g->pl[p].pos >= 0 && g->pl[p].pos < NSPACES, "player %d off the board", p);
    }
    for (int sp = 0; sp < NSPACES; sp++) {
        int o = g->owner[sp];
        CHECK(o == NOBODY || (o >= 0 && o < g->nplayers), "bad owner on %d", sp);
        CHECK(o == NOBODY || board_has_card(sp), "space %d owned but has no card", sp);
        CHECK(g->houses[sp] >= 0 && g->houses[sp] <= 9, "bad house count on %d", sp);
        CHECK(!g->houses[sp] || board[sp].type == T_LOT, "houses off a lot at %d", sp);
        CHECK(!g->houses[sp] || g->house_owner[sp] != NOBODY, "houses without owner at %d", sp);
        if (g->advanced) CHECK(g->houses[sp] <= 3, "more than 3 houses under advanced rules at %d", sp);
    }
}

static void check_hook(Game *g) { consistent(g); }

/* Play one game; returns the winner. */
static int play(int np, const int *diffs, unsigned long seed, int advanced, int force_st, int *turns, int *st)
{
    static Game g;
    GameSettings s;
    memset(&s, 0, sizeof s);
    s.nplayers = np;
    s.all_cpu = 1;
    s.advanced = advanced;
    s.end_wages = 5;
    s.seed = seed;
    for (int p = 0; p < np; p++) {
        s.difficulty[p] = diffs[p];
        snprintf(s.names[p], NAME_LEN, "P%d", p);
    }
    game_new(&g, &s);
    for (int p = 0; p < np; p++) game_set_decider(&g, p, &ai_decider);
    g.hooks.pause = NULL;
    g.hooks.changed = NULL;
    game_deal(&g);
    consistent(&g);
    int t = 0;
    while (!g.over && t < 5000) {
        if (force_st && t == 40 && !g.single_tax) {
            /* Force the Single Tax in by a proposal everyone supports. */
            int save = g.pl[1].difficulty;
            g.pl[1].difficulty = DIFF_MEDIUM;
            if (game_leader(&g) == 1) g.pl[1].difficulty = save;
            game_propose_single_tax(&g, 0);
            if (!g.single_tax) { g.single_tax = 0; }
            g.pl[1].difficulty = save;
        }
        game_turn(&g);
        check_hook(&g);
        t++;
    }
    CHECK(g.over, "game did not end in 5000 turns (seed %lu)", seed);
    *turns = t;
    *st = g.single_tax;
    return game_leader(&g);
}

static void match(const char *title, int a, int b, int games, int advanced)
{
    int wins[2] = { 0, 0 }, total_turns = 0;
    for (int i = 0; i < games; i++) {
        int d[2], swap = i & 1;                 /* alternate who goes first */
        d[0] = swap ? b : a;
        d[1] = swap ? a : b;
        int turns, st;
        int w = play(2, d, 1000 + (unsigned long)i, advanced, 0, &turns, &st);
        total_turns += turns;
        wins[(w == 0) == !swap ? 0 : 1]++;
    }
    if (verbose) printf("%-28s %3d : %3d   (avg %d turns)\n", title, wins[0], wins[1], total_turns / games);
    CHECK(wins[0] > wins[1], "%s: the stronger player won only %d of %d", title, wins[0], games);
}

int main(int argc, char **argv)
{
    verbose = argc > 1 && strcmp(argv[1], "-v") == 0;

    match("hard vs easy", DIFF_HARD, DIFF_EASY, 200, 0);
    match("medium vs easy", DIFF_MEDIUM, DIFF_EASY, 200, 0);
    match("hard vs medium", DIFF_HARD, DIFF_MEDIUM, 1000, 0);   /* close: many games */
    match("hard vs easy (advanced)", DIFF_HARD, DIFF_EASY, 100, 1);

    /* Four players, mixed, both rule sets, with and without the Single Tax. */
    int sts = 0;
    for (int i = 0; i < 100; i++) {
        int d[4] = { i % 3, (i + 1) % 3, (i + 2) % 3, DIFF_HARD };
        int turns, st;
        play(4, d, 7000 + (unsigned long)i, i & 1, i % 3 == 0, &turns, &st);
        sts += st;
    }
    if (verbose) printf("4-player games under the Single Tax: %d of 100\n", sts);
    CHECK(sts > 0, "the Single Tax never came into force");

    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    if (verbose) printf("all checks passed\n");
    return 0;
}
