# Changelog

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
