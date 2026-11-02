# Changelog

## 0.0.16 - 2026-11-02

- **Server mode**: `XLandlord --server` hosts games with no window and no X
  display. It never plays: all four seats are for joiners (as in Host
  Only), the computer player taking a free seat. A game starts as soon as
  four have joined, or 60 seconds after the latest arrival if at least one
  has (`--start-wait SECS`). When it ends a new table opens, game after
  game, until Ctrl+C or SIGTERM, which tells the players of a game under way
  that the server is shutting down.
- Everything the server sees - joins, refusals, drops and returns, every
  line of the record of events, the final standings - is printed to the
  console with the time, and written to a log file per game,
  `~/log/xlandlord-YYYY-MM-DD-HHMM.log` (`-2`, `-3`... if several start in
  one minute; the directory is made if missing).
- Server options, defaulting to the preferences: `--port`, `--skill`,
  `--wages`, `--advanced` / `--standard`, `--reconnect`, `--answer`,
  `--start-wait`, `--name`.
- Fix: a player kept busy in a dialog while many messages arrived could
  lose them all (the receive buffer was emptied when it filled up), among
  them the host's goodbye. Now a full buffer is emptied only when it holds
  one absurdly long line.
- Fix: after a game had ended, closing the table recorded the players as
  leaving it.
- `make check` runs tests/server.sh: the server starts with no display,
  writes its console and log lines, and stops on SIGTERM.
- The man page has a SERVER MODE section; README, INSTALL, the rule book
  and Help > Network Play describe it.

## 0.0.15 - 2026-11-02

- Hosting has a new first choice: **Host and Play** (as before: the host
  takes seat 1 and up to three others join) or **Host Only**: the host runs
  the game without a seat and watches it on his screen - board, player boxes
  and record of events - with no turn of his own (he can still remove a
  silent player or end the game). All four seats are then for joiners; the
  computer player still takes a free seat, at the host's skill, and gives
  it up to a fourth joiner before the start. Start Game needs one joiner at
  least (one alone plays the computer player).
- A watching host's name does not block a joiner from using it.
- The waiting rooms say "hosted by Lee on gamebox (not playing)".
- New preference `net_host_only`.
- Network protocol 7 (START carries the host's seat, -1 when he only
  hosts): 0.0.15 cannot play with 0.0.14.
- tests/net plays host-only games with one joiner (against the computer
  player) and with four (no computer player; a fifth turned away).

## 0.0.14 - 2026-11-02

- A title card at start-up: it fills the window, on the board's cream with
  its double rule - pictures from the board, "XLandlord" in large type,
  "The Landlord's Game", Lizzie Magie's patent of 1904 and the rules of
  1906, the motto "Labor upon Mother Earth produces Wages", the version and
  copyright.
- The first question is on the title card: Host a Network Game, Join a
  Network Game, Play Here or Quit (click, the underlined letter, or the
  arrows and Enter; a line says what the chosen one does).
- The order is now: how to play first, then the player's name. Cancel at
  the name prompt goes back to the title card.
- With --host, --join or --local the title card shows for two seconds (any
  key skips it), then the name prompt.
- xw_text_scaled draws large titles from the ordinary X fonts.
- The rule book's start-up picture, the guide's Starting page and every
  description of start-up follow the new order.
- The rules PDF prints the name-prompt picture at a sensible size (it was
  stretched to the page width).

## 0.0.13 - 2026-11-01

Documentation, rules and comments brought up to date with 0.0.10 - 0.0.12:

- README: the name prompt described for every game (not only network
  play); hosting lists the timers; a Names paragraph (login name vs. player
  name, no duplicates); rejoining after a drop is the one way into a game
  under way; the source-layout table brought up to date.
- Rule book (and PDF): a screenshot of the name prompt; section 20 now
  records the decisions made for play with the computer and over the
  network (one computer player and who sets its skill, names, starting
  money, leaving, dropped connections, answer time, the host); a Network
  line on the quick-reference page.
- Man page: playing at the screen mentions the name prompt and seat 1.
- INSTALL: the ssh user vs. the player's name; troubleshooting for a
  different version, Name Taken, Connection Lost and Are You Still There.
- Help > About mentions playing with others at the screen and over the
  network.
- Code comments: every function has its comment again (some had been
  separated from their functions by later edits); the overviews at the top
  of main.c, game.c and netui.h describe start-up, players leaving, the
  network timers and reconnecting.

## 0.0.12 - 2026-11-01

- The player's name in the game is no longer the system's user name. At
  every start the game first asks "Your Name" (the name used last time is
  filled in; Enter keeps it, an empty name is asked again, Cancel quits).
  That name is the player's identity at the table and in network games;
  seat 1 of the New Game form uses it without asking again.
- The system's user name is still what ssh logs in with (and what the
  host's waiting room suggests as user@host), but it is not tied to any
  player's name.
- Network play: no two players may share a name (letter case aside). A
  joiner whose name is taken by the host or by someone in the waiting room
  is turned away and asked for another name, then joins again.
- tests/net checks that duplicate names are turned away.

## 0.0.11 - 2026-11-01

- A dropped connection is no longer the end: the joiner's seat waits for
  him for the host's "Wait for a dropped player" time (1, 2 or 5 minutes,
  default 2). His game reconnects by itself every few seconds (with a
  countdown and Give Up); the host lets back in the same name from the same
  machine and asks again the question that was cut off. Play pauses when it
  needs him, with a countdown on every screen. Not back in time: he leaves
  the game. A deliberate quit (BYE) still leaves at once.
- Taking too long: each question and each turn has the host's "Time to
  answer" (1, 2 or 5 minutes, default 2). Then the joiner's game asks "Are
  you still there?" with a one-minute countdown; "I'm Still Here" starts
  his time again. If he does not answer he is taken out so play can go on,
  and told why. While waiting, the host sees a panel with the time left and
  a Remove button.
- The host leaving ends the game for everyone, and every joiner is told -
  also when the host's game stops without a goodbye (found while
  reconnecting).
- Starting money is documented as set once at the start (1906 table).
- Timers work under any open dialog (an idle function in the X event
  wait). New preferences `net_reconnect` and `net_answer`.
- Network protocol 6 (START carries the timers; ALIVE and WAIT are new):
  0.0.11 cannot play with 0.0.10.
- tests/net: a joiner who drops and comes back keeps his seat; one who does
  not come back leaves; one who stops answering is taken out and told.

## 0.0.10 - 2026-10-31

- One computer player at most, at any table. Its skill (Easy, Medium or
  Hard) is set for the whole game by whoever sets the game up - the New
  Game form at this screen, the host's setup in a network game - and no
  player chooses a skill for himself.
- The New Game form: seats are Empty, Human or Computer (one Computer seat
  at most), with a single Computer skill row.
- Network play: once the game has started nobody new joins and the computer
  player stays to the end. A joiner who leaves, or whose connection drops,
  is out of the game: his cards go back into the pack, his houses come
  down, his cash goes with him, and nobody takes his seat (until now a
  medium computer player took over, which could make a second one). His
  box shows that he has left; standings list him last. With one player
  left the game is over. Before the start, a fourth human still takes the
  computer player's seat.
- Preferences: `seatN = computer` and one `cpu_skill`, used here and for
  hosted games, replace the per-seat levels and `net_cpu`; older files are
  converted (the first computer seat is kept, at its level).
- Network protocol 5 (the state says who has left): 0.0.10 cannot play
  with 0.0.9.
- tests/engine checks a player leaving mid-game; tests/net checks that a
  quitter's seat is not taken and exactly one computer player remains.

## 0.0.9 - 2026-10-31

- The printable rule book, `xlandlord-rules.pdf`, stays: it is built and
  installed as before, and the man page (THE RULE BOOK (PDF)), Help > The
  Rules of 1906, the last page of Help > How to Play, README, INSTALL and
  the rule book itself say where it is installed and how to view, print
  and rebuild it. The program is told the installed path (`DOCDIR`).

## 0.0.8 - 2026-10-31

- A full manual page, `man 6 XLandlord`, installed with the game: the
  command line, building and installing, setting up password-less ssh for
  network play, every preferences key, playing at the screen, keys, and the
  complete rules of 1906 with the advanced rules and the Single Tax.
- Help > How to Play (F1) is now an illustrated guide: twenty pages, each a
  screenshot from the game with a short explanation, turned with Back /
  Next or the arrow keys. The window fits the screen; text that does not
  fit scrolls.
- The screenshots are compiled into the program (`src/guidepics.c`, about
  350 KB), made from `docs/guide/*.png` by `docs/guidepics.c`;
  `make guide-pics` remakes them.
- The man page no longer mentions the old program name.

## 0.0.7 - 2026-10-31

- The game is now called **XLandlord** (it was "Viola the Landlord"), and
  so is the program: `XLandlord`. The tarball, documents folder, icons and
  menu entry use the lower-case `xlandlord`; the man page is
  `man 6 XLandlord`; the rule book is `docs/xlandlord-rules.pdf`. The look is
  still the Viola design style.
- Preferences move to `~/.config/xlandlord/preferences.conf`. The first
  time XLandlord runs it reads `~/.config/viola-landlord/preferences.conf`,
  if there is one, and saves it under the new name (a `net_command` of
  `viola-landlord` becomes `XLandlord`).
- For network play every machine needs XLandlord 0.0.7: the joiner's ssh
  now runs `XLandlord --relay` on the host. The ssh stand-in variable for
  testing is now `XLANDLORD_SSH`.
- The project is on GitHub: https://github.com/dleecurt/XLandlord

## 0.0.6 - 2026-10-31

- Each player's machine is shown next to his name: in the host's waiting
  room ("Ann (from annbox)"), in the joiners' waiting room, and in the
  player boxes during the game ("Lee (host, gamebox)", "Ann (you, annbox)",
  "Bea (network, 203.0.113.45)"). The host shows its own host name.
- A joiner's machine is the client address of his ssh connection: the relay
  reads it from `SSH_CONNECTION` and sends it to the host as the first line
  (`FROM`); the host looks the address up in its hosts file and DNS, and
  shows the IP address when there is no name. Only a connection's first
  line may be a FROM, so a player cannot claim to be somewhere else.
- The waiting room's message line says where a new player joined from.
- Network protocol 4: 0.0.6 cannot play with 0.0.5. (A 0.0.5 relay still
  works; its joiners show as "from an unknown machine".)
- tests/net checks the labels: a named address, an address with no name,
  and a relay not started by sshd.

## 0.0.5 - 2026-10-31

- At start-up the game asks how to play: Host a Network Game, Join a
  Network Game, or Play Here. `--host`, `--join HOST` and `--local` on the
  command line skip the question.
- Joining by IP address (IPv4 or IPv6) or by host name. Names are looked up
  with the system resolver - the hosts file and DNS, as /etc/nsswitch.conf
  orders them - before ssh is started; an unknown name gets a plain "Host
  Not Found" with advice (use the IP address, or add it to /etc/hosts).
  ssh is still given the name as typed, so known_hosts entries keep working.
- Addresses may carry `user@` and `:port`; IPv6 with a port as
  `[2001:db8::5]:1905`. Malformed addresses are refused with a reason.
- The host's waiting room shows the machine's host name and its IP
  addresses (loopback and link-local left out).
- tests/net checks address parsing and lookups, and its robot players join
  alternately by name and by IP address.

## 0.0.4 - 2026-10-31

- INSTALL.md: a full build and install guide (packages for Debian/Ubuntu,
  Fedora, Arch and NetBSD, configure options, make targets, what is
  installed where, uninstalling, network-play needs, troubleshooting).
- docs/RULES.md: the complete rule book and how-to-play guide, with
  screenshots, a numbered board map, the history of the game and a
  quick-reference page; and docs/xlandlord-rules.pdf made from it.
- `make pdf` rebuilds the PDF (and the board map, from the game's own board
  data, by docs/boardmap.c) when configure finds pandoc and pdflatex; the
  PDF ships ready-made otherwise.
- `make install` installs the documents in $(docdir).
- Every function in the sources now has a comment saying what it does.

## 0.0.3 - 2026-10-31

- Network play over ssh. One player hosts (Game > Host a Network Game) and
  up to three join (Game > Join a Network Game, giving `[user@]host`). The
  joiner's game runs `ssh -T -o BatchMode=yes host XLandlord --relay
  PORT`; the relay connects to the hosting game, which listens on localhost
  only. Password-less ssh keys are assumed; README.md and Help > Network Play
  explain how to set them up.
- Four players at most: a fifth who tries to join is turned away with a
  warning, as is anyone joining a game already under way.
- One computer player takes a free seat when fewer than four humans play;
  four humans play with no computer player.
- A waiting room for the host (who has joined, Start Game) and for joiners.
- Joiners see the same board and dialogs; their builds, trades, sales,
  Single Tax proposals and throws go to the host. Trades work between
  players on different machines.
- A joiner whose connection drops is replaced by a computer player (medium).
- New preferences: `net_host`, `net_port`, `net_command`, `net_cpu`.
- `tests/net` now plays whole network games through a stand-in for ssh.

## 0.0.2 - 2026-10-31

- Local (hot-seat) multiplayer: the New Game form now has four seats, each
  Empty, Human, Easy, Medium or Hard, so humans and computer players can sit
  at one table (two to four players). Each human seat has its own name.
- A "Pass the game to ..." prompt when the screen moves from one human to
  another, and every dialog names the player it is asking ("Lee: For Sale")
  whenever more than one human is playing.
- Trades between humans: buying a card from another human, or offering one
  to him at a price you name, asks him to accept or decline.
- Network play is stubbed out: Game > Host / Join a Network Game, the
  protocol (src/net.h), its message format (tested), and a network seat that
  the computer plays until its player is connected. The sockets and the
  lobby are still to do; the menus say so.
- Preferences: `seat1`..`seat4` and `name1`..`name4` replace `difficulty`,
  `opponents` and `name`; 0.0.1 files are read and converted.
- `make check` also runs tests/net.

## 0.0.1 - 2026-10-31

First release.

- The Landlord's Game by the 1906 rules (Economic Game Co.), with the Single
  Tax rules and the optional "advanced and scientific" rules.
- One to three computer players at Easy, Medium or Hard.
- X11 interface in plain Xlib in the Viola design style: menu bar, dialogs,
  check lists, forms, the board drawn after the 1904 patent layout.
- Original pixel art (GPL-3.0-or-later and CC0 1.0).
- Preferences in `~/.config/xlandlord/preferences.conf`.
- `make check`: hundreds of computer-only games test the rules engine and
  the order of the difficulty levels.
