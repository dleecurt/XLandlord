# Changelog

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
  quick-reference page; and docs/viola-landlord-rules.pdf made from it.
- `make pdf` rebuilds the PDF (and the board map, from the game's own board
  data, by docs/boardmap.c) when configure finds pandoc and pdflatex; the
  PDF ships ready-made otherwise.
- `make install` installs the documents in $(docdir).
- Every function in the sources now has a comment saying what it does.

## 0.0.3 - 2026-10-31

- Network play over ssh. One player hosts (Game > Host a Network Game) and
  up to three join (Game > Join a Network Game, giving `[user@]host`). The
  joiner's game runs `ssh -T -o BatchMode=yes host viola-landlord --relay
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
- Preferences in `~/.config/viola-landlord/preferences.conf`.
- `make check`: hundreds of computer-only games test the rules engine and
  the order of the difficulty levels.
