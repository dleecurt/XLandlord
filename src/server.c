/*
 * XLandlord - The Landlord's Game for X11
 * Copyright (C) 2026 Lee Curtis
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

/* server.c - see server.h: XLandlord --server, a host with no window.
 *
 * THE SERVER'S LOOP
 *   1. Open a log file, ~/log/xlandlord-YYYY-MM-DD-HHMM.log (the directory
 *      is made if missing), and a table: listen on 127.0.0.1:port, the host
 *      not playing (net_set_host_plays(0)), so all four seats are for
 *      joiners.
 *   2. The waiting room: joiners come and go (each event printed and
 *      logged). The game starts at once when four have joined, or
 *      start_wait seconds after the latest arrival if at least one has.
 *   3. The game: joiners' seats are played through net_decider, the
 *      computer player (if a seat was free) by the AI. Every line of the
 *      record of events goes to the console and the log, with the time.
 *      The timers (reconnect, answer) work as for any host; there is
 *      nobody to press Remove, so only they take a silent player out.
 *   4. The final standings go to the console and the log; the joiners are
 *      told the game is over; the log is closed; back to 1.
 *   Ctrl+C (SIGINT) or SIGTERM ends it: the joiners are told the server is
 *   shutting down, and the log is closed.
 *
 * No X calls are made: app.headless is set, so the code shared with the
 * windowed host (main.c's handling of joiners' actions) does not draw. */

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "server.h"
#include "ai.h"
#include "app.h"
#include "board.h"
#include "net.h"
#include "prefs.h"
#include "util.h"
#include "version.h"

static volatile sig_atomic_t stop_requested;  /* Ctrl+C or SIGTERM */
static FILE *logf;                            /* this game's log file, or NULL */
static int printed;                           /* lines of the record already printed */
static int frozen;                            /* shutting down: the record is no longer printed */

/* The signal handler: ask the loops to stop. */
static void on_signal(int sig) { (void)sig; stop_requested = 1; }

/* One line to the console and the log, with the time in front. */
static void say(const char *fmt, ...)
{
    char text[1200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%H:%M:%S", &tm);
    printf("[%s] %s\n", stamp, text);
    fflush(stdout);
    if (logf) {
        strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", &tm);
        fprintf(logf, "[%s] %s\n", stamp, text);
        fflush(logf);
    }
}

/* Open a new log file for a game: ~/log/xlandlord-YYYY-MM-DD-HHMM.log,
 * with -2, -3... if that name is taken; the directory is made if needed.
 * Without it, the server still runs (console only) and says why. */
static void open_log(void)
{
    if (logf) { fclose(logf); logf = NULL; }
    const char *home = getenv("HOME");
    if (!home || !*home) { say("No HOME directory: not writing a log file."); return; }
    char dir[600], path[700];
    snprintf(dir, sizeof dir, "%s/log", home);
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        say("Cannot make %s (%s): not writing a log file.", dir, strerror(errno));
        return;
    }
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char stamp[32];
    strftime(stamp, sizeof stamp, "%Y-%m-%d-%H%M", &tm);
    for (int n = 1; n < 100; n++) {
        if (n == 1) snprintf(path, sizeof path, "%s/xlandlord-%s.log", dir, stamp);
        else snprintf(path, sizeof path, "%s/xlandlord-%s-%d.log", dir, stamp, n);
        if (access(path, F_OK) != 0) break;
    }
    logf = fopen(path, "w");
    if (!logf) { say("Cannot write %s (%s): not writing a log file.", path, strerror(errno)); return; }
    say("XLandlord %s server: logging to %s", PACKAGE_VERSION_STR, path);
}

/* Print the record's new lines (the game's "changed" hook), and send the
 * joiners the new state. */
static void changed(Game *g)
{
    if (frozen) { printed = g->log_count; return; }     /* (what the engine does as it winds down) */
    int first = MAX(printed, g->log_count - LOG_LINES);
    for (int i = first; i < g->log_count; i++) say("%s", g->log[i % LOG_LINES]);
    printed = g->log_count;
    net_broadcast(g);
}

/* Stopping: the game ends where it stands (so the joiners are not
 * recorded as leaving it), and the waits give up. */
static int stopping(void)
{
    if (!stop_requested) return 0;
    if (app.playing && !app.game.over) {
        changed(&app.game);                 /* the record so far */
        app.game.over = 1;
        frozen = 1;
        say("The server is shutting down: the game ends here.");
    }
    return 1;
}

/* While net.c waits on the network: 1 stops the wait (the server is
 * shutting down). */
static int server_idle(void) { return stopping(); }

/* While the game waits for a player who is away or has not answered: say
 * so now and then (there is nobody to press Remove); 2 ends the game. */
static int server_wait(int p, int why, int secs)
{
    static int last_p = -1, last_why = -1, last_said = -1;
    if (stopping()) return 2;
    if (p != last_p || why != last_why || (secs % 30 == 0 && secs != last_said)) {
        say(why == NW_RECONNECT ? "Waiting for %s to reconnect: %d:%02d left."
                                : "Waiting for %s to answer (he is being asked if he is still there): %d:%02d left.",
            app.game.pl[p].name, secs / 60, secs % 60);
        last_p = p; last_why = why; last_said = secs;
    }
    usleep(100000);
    return 0;
}

/* Print what the network layer has to report (joins, refusals, departures). */
static void network_events(void)
{
    for (const char *e; (e = net_host_event()); ) say("%s", e);
}

/* The final standings, richest first (players who left last). */
static void standings(const Game *g)
{
    int order[MAXPLAYERS], n = g->nplayers;
    for (int i = 0; i < n; i++) order[i] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0; j--) {
            int a = order[j], b = order[j - 1];
            int wa = game_active(g, a) ? game_worth(g, a) : -1, wb = game_active(g, b) ? game_worth(g, b) : -1;
            if (wa <= wb) break;
            order[j] = b; order[j - 1] = a;
        }
    say("Final standings (turns played: %d):", g->turn_no);
    for (int i = 0; i < n; i++) {
        const Player *P = &g->pl[order[i]];
        if (P->left) { say("  %-18s left the game", P->name); continue; }
        say("  %-18s worth $%d  (cash $%d, %d cards, %d houses, %d luxuries)", P->name, game_worth(g, order[i]),
            P->cash, game_cards_of(g, order[i], -1), game_houses_of(g, order[i]), P->luxuries + P->professors);
    }
    say("%s is the winner - the millionaire!", g->pl[order[0]].name);
}

/* The waiting room: 1 when the game should start, 0 if the server is
 * stopping. */
static int wait_for_players(const ServerOptions *o)
{
    int seen = 0;
    time_t last_arrival = 0, last_note = 0;
    for (;;) {
        if (stop_requested) return 0;
        net_host_poll(200);
        network_events();
        int n = net_lobby_count();
        time_t now = time(NULL);
        if (n != seen) {
            if (n > seen) last_arrival = now;
            seen = n;
            if (n > 0 && n < MAXPLAYERS)
                say("%d player%s waiting; the game starts in %d s unless someone else joins.", n, n == 1 ? "" : "s",
                    o->start_wait);
        }
        if (n >= MAXPLAYERS) { say("The table is full."); return 1; }
        if (n > 0 && now - last_arrival >= o->start_wait) return 1;
        if (n > 0 && now - last_note >= 15 && now - last_arrival < o->start_wait && now != last_arrival) {
            say("Starting in %d s...", (int)(o->start_wait - (now - last_arrival)));
            last_note = now;
        }
    }
}

/* One game, from opening the table to its end. 0 if the server should
 * stop. */
static int serve_one(const ServerOptions *o)
{
    open_log();
    char err[400];
    net_set_host_plays(0);
    net_set_host_name(o->name);
    net_set_timers(60 * o->reconnect_min, 60 * o->answer_min);
    if (net_host(o->port, err, sizeof err) < 0) { say("Cannot host: %s", err); return 0; }
    say("Waiting for players on port %d (hosted by %s; computer player %s; the game ends at %d wages; %s rules).",
        o->port, o->name, o->skill == DIFF_EASY ? "easy" : o->skill == DIFF_HARD ? "hard" : "medium", o->end_wages,
        o->advanced ? "advanced" : "standard");
    if (!wait_for_players(o)) { net_close("The server is shutting down."); return 0; }

    GameSettings s;
    memset(&s, 0, sizeof s);
    s.advanced = o->advanced;
    s.end_wages = o->end_wages;
    s.seed = (unsigned long)time(NULL) ^ (unsigned long)getpid();
    net_build_table(&s, o->name, o->skill);
    Game *g = &app.game;
    game_new(g, &s);
    g->hooks.changed = changed;
    printed = g->log_count;
    for (int p = 0; p < g->nplayers; p++) game_set_decider(g, p, g->pl[p].cpu ? &ai_decider : &net_decider);
    net_set_action_handler(app_host_action);
    app.playing = 1;
    char who[400] = "";
    for (int p = 0; p < g->nplayers; p++) {
        char one[100];
        snprintf(one, sizeof one, "%s%s%s", p ? ", " : "", g->pl[p].name, g->pl[p].cpu ? " (computer)" : "");
        strncat(who, one, sizeof who - strlen(who) - 1);
    }
    say("The game begins: %s.", who);
    net_start_game(g);
    game_deal(g);
    changed(g);
    while (!g->over && !stop_requested && net_role() == NET_HOST) {
        game_turn(g);
        changed(g);
        network_events();
    }
    changed(g);
    app.playing = 0;
    if (stop_requested) {
        stopping();
        net_close("The server is shutting down; the game is over.");
        return 0;
    }
    if (g->over) standings(g);
    net_close("The game is over. Thank you for playing.");
    say("The game is over.");
    return 1;
}

/* Read a number option's value; 0 and a message on error. */
static int number_arg(int argc, char **argv, int *i, int lo, int hi, int *out)
{
    if (*i + 1 >= argc) { fprintf(stderr, "XLandlord: %s needs a number\n", argv[*i]); return 0; }
    char *end;
    long v = strtol(argv[*i + 1], &end, 10);
    if (*end || v < lo || v > hi) {
        fprintf(stderr, "XLandlord: %s must be %d to %d\n", argv[*i], lo, hi);
        return 0;
    }
    *out = (int)v;
    (*i)++;
    return 1;
}

/* See server.h. */
int server_parse(int argc, char **argv, int i, ServerOptions *o)
{
    const char *a = argv[i];
    if (!strcmp(a, "--port")) return number_arg(argc, argv, &i, 1, 65535, &o->port) ? 2 : -1;
    if (!strcmp(a, "--wages")) {
        int w;
        if (!number_arg(argc, argv, &i, 1, 99, &w)) return -1;
        if (w != 3 && w != 5 && w != 7 && w != 10) { fprintf(stderr, "XLandlord: --wages is 3, 5, 7 or 10\n"); return -1; }
        o->end_wages = w;
        return 2;
    }
    if (!strcmp(a, "--reconnect") || !strcmp(a, "--answer")) {
        int m;
        if (!number_arg(argc, argv, &i, 1, 5, &m)) return -1;
        if (m != 1 && m != 2 && m != 5) { fprintf(stderr, "XLandlord: %s is 1, 2 or 5 (minutes)\n", a); return -1; }
        if (a[2] == 'r') o->reconnect_min = m; else o->answer_min = m;
        return 2;
    }
    if (!strcmp(a, "--start-wait")) return number_arg(argc, argv, &i, 5, 3600, &o->start_wait) ? 2 : -1;
    if (!strcmp(a, "--skill")) {
        if (i + 1 >= argc) { fprintf(stderr, "XLandlord: --skill needs easy, medium or hard\n"); return -1; }
        const char *v = argv[i + 1];
        if (!strcasecmp(v, "easy")) o->skill = DIFF_EASY;
        else if (!strcasecmp(v, "medium")) o->skill = DIFF_MEDIUM;
        else if (!strcasecmp(v, "hard")) o->skill = DIFF_HARD;
        else { fprintf(stderr, "XLandlord: --skill is easy, medium or hard\n"); return -1; }
        return 2;
    }
    if (!strcmp(a, "--name")) {
        if (i + 1 >= argc || !argv[i + 1][0]) { fprintf(stderr, "XLandlord: --name needs a name\n"); return -1; }
        copy_str(o->name, sizeof o->name, argv[i + 1]);
        for (char *q = o->name; *q; q++) if (*q == '|') *q = '/';
        return 2;
    }
    if (!strcmp(a, "--advanced")) { o->advanced = 1; return 1; }
    if (!strcmp(a, "--standard")) { o->advanced = 0; return 1; }
    return 0;
}

/* See server.h. */
void server_defaults(ServerOptions *o, const Prefs *p)
{
    memset(o, 0, sizeof *o);
    o->port = p->net_port;
    o->skill = CLAMP(p->cpu_skill, DIFF_EASY, DIFF_HARD);
    o->end_wages = p->end_wages;
    o->advanced = p->advanced;
    o->reconnect_min = p->net_reconnect;
    o->answer_min = p->net_answer;
    o->start_wait = 60;
    copy_str(o->name, sizeof o->name, "XLandlord server");
}

/* See server.h. */
int server_run(const ServerOptions *o)
{
    app.headless = 1;
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    net_set_idle(server_idle);
    net_set_wait_hook(server_wait);
    printf("XLandlord %s server. Players join with Join a Network Game; Ctrl+C stops the server.\n",
           PACKAGE_VERSION_STR);
    while (!stop_requested && serve_one(o)) ;
    say("The server has stopped.");
    if (logf) { fclose(logf); logf = NULL; }
    return 0;
}
