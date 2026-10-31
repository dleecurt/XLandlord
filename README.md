# Viola the Landlord

**The Landlord's Game**, the board game Lizzie J. Magie patented in 1904
(US Patent 748,626) and the ancestor of Monopoly, for X11, played against the
computer. It follows the rules published by the Economic Game Company of New
York in 1906, **including the Single Tax rules**, and is written in C with
plain Xlib. The window is drawn in the style of the standalone **Viola** web
browser, like Ted Viola: flat gray94 panels, 2px light and dark bevels, 1px
black boxes, Helvetica menus with Viola's small triangles, black focus
rectangles on the buttons.

The project is called **Viola the Landlord**; the program you run is
`viola-landlord`.

Viola the Landlord is free software, released under the **GNU General Public
License, version 3 or (at your option) any later version**. See
[License](#license).

## Quick start

From a release tarball (`viola-landlord-X.Y.Z.tar.gz`) you need a C compiler,
make and the X11 development files:

```
./configure     # checks the system (GNU autoconf); --prefix=DIR chooses where to install
make            # builds ./viola-landlord
make check      # plays hundreds of computer-only games to test the rules (no display needed)
make install    # into /usr/local by default (needs privileges); man page:  man 6 viola-landlord
make uninstall
make clean

./viola-landlord
./viola-landlord --version
```

From a source checkout (no `configure` yet), run `./autogen.sh` first; it
needs GNU autoconf, automake and pkg-config.

| System        | Build                           | Fonts (to look like Viola)                  |
| ------------- | ------------------------------- | ------------------------------------------- |
| Debian/Ubuntu | `sudo apt install libx11-dev`   | `sudo apt install xfonts-base xfonts-75dpi` |
| Fedora/RHEL   | `sudo dnf install libX11-devel` | `xorg-x11-fonts-misc xorg-x11-fonts-75dpi`  |
| NetBSD        | the X sets (xbase, xcomp)       | the X font set (xfont)                      |

Without the 75dpi fonts the game still runs; X picks the nearest fonts it has.

Current version: **0.0.3** (see `CHANGELOG.md`).

## Playing

A new game starts with the **New Game** form: four seats, each **Empty,
Human, Easy, Medium or Hard** (two to four players, at least one human), how
many times a player draws his wages before the game ends (the 1906 rules say
five) and the rules (standard, or the 1906 "advanced and scientific" rules).
Each human seat is asked for its player's name. Then the cards are dealt
(Rule 1) and each player ticks the ones he wants to buy.

### Several players at one screen (hot-seat)

Make two or more seats Human. They take turns at the keyboard and mouse:

- when the turn passes from one human to another, a **Pass the game to ...**
  prompt says whose turn it is;
- every question names the player it is for ("Lee: For Sale (Rule 7)"),
  because some come up on other players' turns - bids, Single Tax votes, the
  Trust, money owed to another player;
- humans can trade with each other: **Buy a Card** offers a price to the
  owner, who accepts or declines; **Sell a Card** lists the computer players'
  offers and lets you name a price to any human.

### Network play (over ssh)

One player **hosts** and up to three others **join** from their own machines.
The connection goes through **ssh**; the game itself never opens a port to the
network.

**Hosting.** Game > Host a Network Game. Choose the strength of the computer
player, the length of the game, the rules and a port (1904 by default; it is
only reachable from the host's own machine). A waiting room lists who has
joined; press **Start Game** when everyone is in.

**Joining.** Game > Join a Network Game, and enter the host's machine the way
ssh wants it: `host` or `user@host`, plus `:port` if the host chose another
port than 1904. You see the same board and the same dialogs as at the host; on
your turn the Roll / Build / Trade buttons work. The menus and dialogs say whose
turn it is.

**The table.**

- Four players at most: the host and three joiners. A fifth player who tries to
  join is turned away with a warning ("The table is full..."); so is anyone who
  tries to join a game that has already started.
- If fewer than four humans play, **one** computer player takes a free seat.
  With four humans there is no computer player.
- If a joiner's connection drops, a computer player (medium) takes over his
  seat and the game goes on. If the host leaves, the game ends for everyone.

**How it works.** The joiner's program runs

```
ssh -T -o BatchMode=yes user@host viola-landlord --relay 1904
```

On the host, ssh starts a second copy of viola-landlord as a *relay*, which
connects to the hosting game on `127.0.0.1:1904` and copies messages both ways.
The host runs the only real game; joiners draw what it sends and answer its
questions. `BatchMode=yes` means ssh will never stop to ask for a password, so
**password-less (key) access is required**. The protocol is described in
`src/net.h`.

#### Setting up password-less ssh

The game assumes each joiner can already ssh to the host without typing a
password; it does not set this up for you. The usual way, on **each joiner's
machine**:

1. The host needs an ssh server running (`sudo apt install openssh-server`
   on Debian/Ubuntu; on NetBSD, `sshd=YES` in `/etc/rc.conf`).
2. Make a key pair if you have none. Press Enter for an empty passphrase, or
   choose a passphrase and load the key into `ssh-agent` (`ssh-add`) before
   playing:

   ```
   ssh-keygen -t ed25519
   ```

3. Install the public key on the host (this asks for the host password one last
   time):

   ```
   ssh-copy-id user@host
   ```

   Without `ssh-copy-id`, append `~/.ssh/id_ed25519.pub` to
   `~/.ssh/authorized_keys` on the host by hand.
4. Accept the host's key once with a plain `ssh user@host`, then check that
   this prints the version **without asking anything**:

   ```
   ssh -o BatchMode=yes user@host viola-landlord --version
   ```

If step 4 fails:

- "Permission denied": the key is not in the host's
  `~/.ssh/authorized_keys`, or the permissions are too open (`chmod 700 ~/.ssh`
  and `chmod 600 ~/.ssh/authorized_keys` on the host).
- "Host key verification failed": do the plain `ssh user@host` once.
- "command not found": non-interactive ssh does not see the host's PATH (for
  instance `/usr/local/bin`). Either fix the PATH for non-interactive shells on
  the host, or set `net_command = /usr/local/bin/viola-landlord` (the full path
  on the host) in the joiner's `~/.config/viola-landlord/preferences.conf`.
- Every machine needs the **same version** of Viola the Landlord; a different
  version is turned away with a message.

To use a different user name, port or key for ssh, put a `Host` entry in the
joiner's `~/.ssh/config`; the game uses whatever ssh does.

On your turn press **Roll Dice** (Space or Enter). Whenever the rules give you
a choice, a dialog asks: buying a lot and bidding for one, paying the Jail fine
or throwing for a double, using the railroad pass a double gives you, moving
backward between the Chances, refusing to speculate, buying a luxury, voting on
the Single Tax, joining a railroad Trust, and what to sell when you are short of
money. Before rolling you may build houses (B), buy a card from a computer
player (T), sell one (S), sell to the pack, or propose the Single Tax (X).
Click any space on the board to read about it. **F1** explains the game,
Help > The Rules of 1906 has the whole rule book as played, and Help > Lizzie
Magie and the Game tells its history.

The richest player at the end wins: cash, plus 100 for every card, every house
and every luxury (and every Professor card under the Single Tax).

### The computer players

| Level  | How it plays |
| ------ | ------------ |
| Easy   | On impulse: buys about half of what it can afford, almost never bids or builds, picks moves at random, keeps little cash. |
| Medium | By rules of thumb: buys and builds while it keeps a $100 cushion, bids a little over the price for good cards, pays out of Jail early in the game, avoids an obviously expensive square. |
| Hard   | By counting: values each card at 100 plus the rent it should bring in the circuits left, keeps in hand what the trip round the board is likely to cost, hunts railroad monopolies and the Municipal Cinch (and blocks yours), bids up to a card's worth, sits in Jail late in the game when the board is dangerous, picks the cheapest square for a pass or a backward move, trades with the other computer players, and votes on the Single Tax by what it does to its standing. |

`make check` plays them against each other. Over the test's games Hard beats
Easy about 96% of the time, Medium beats Easy about 86%, and Hard beats Medium
about 59% (the dice still have their say).

### Options

Options > Game Settings (also your name), Computer Speed (fast, normal,
slow), **Ask Me to Bid** (off: you are never asked to bid when a computer
player lands on an unowned card), **Confirm Each Computer Move** (wait for a key
after every computer turn). They are kept in
`~/.config/viola-landlord/preferences.conf` (`$XDG_CONFIG_HOME` is honored),
with the seats as `seat1` .. `seat4` (`empty`, `human`, `easy`, `medium`,
`hard`) and the human names as `name1` .. `name4`; for network play
`net_host` (the last host joined), `net_port` (the port to host on),
`net_command` (the program ssh runs on the host, normally `viola-landlord`) and
`net_cpu` (the hosted game's computer player). A 0.0.1 file is converted.

## The rules as implemented

The 1906 rules (as transcribed from the Economic Game Co. rule sheet) and the
patent are followed as written: starting money $600 / $500 / $400 for two,
three or four players; $100 wages; $10 taxes on Food, Shelter, Fuel and
Clothing; Jail for trespassing on Lord Blueblood's Estate and D. F. Hogg's Game
Preserves, $50 fine or a double, three turns at most; land rent; bidding for
unowned cards with the first option to the player who stopped there;
Speculation with its payout table and the broker's "skin game"; franchises at
$5, the Municipal Cinch at $25, railroad Monopoly at $10 / $20 / $50 and the
Trust at $40; Luxury at $75 (counts 100) or a backward move; houses at $100 and
$10 each; the official railroad pass on a double; the Poor House; the game's end
at five wages; the advanced rules (taxes doubling at 10 and 25 houses, section
rents doubling with each round of houses, three houses a lot); and the Single
Tax rules 1 to 8 (no indirect taxes, land rent to the treasury, building on any
unimproved lot, the public buying the utilities, the free colleges and the
Education and Professor cards, rising wages, no Poor House).

### What is reconstructed, and why

The surviving 1906 rule text does not say everything, and the rules
themselves leave a few matters "to be settled among the players" (Rule 18).
This program decides them as follows:

- **The order of the spaces.** The rules fix some positions (The Pike is the
  6th space, Beggarman's Court the 14th, Shelter is right after Jail, Luxury is
  six spaces past Madison Square, which is six past Slambang Trolley; the
  railroads divide each side in two; nine spaces lie between corners). The
  other spaces are placed around those, keeping cheap lots near Mother Earth.
- **Land rents.** The 1906 Land Rent Table runs from $2 to $22 in steps of $2;
  the lots get those rents in order round the board.
- **The Legacy space** (from the 1904 patent) and a second Luxury space fill the
  board, which needs one or two more spaces than the 1906 rules name.
- **The Chance cards.** The 1906 red cards have not survived in print, so there
  are sixteen new ones in the spirit of the game.
- **Raising money.** There is no borrowing. A player short of money may sell a
  card to a computer player at its offer, sell a card back to the pack for half
  its sale price, or sell a house to the bank for $50; failing that he goes to
  the Poor House (under the Single Tax: to the Natural Opportunity corner behind
  him, from which he draws $50 the next turn).
- **Bidding** runs as an auction in $5 steps; the winner pays $5 over the
  runner-up's limit.
- **Houses** are limited to nine a lot under the standard rules (three under
  the advanced rules, as the rules say).

## Artwork

Only public-domain material or art made for this program is used:

- The **board** is drawn by the program after the layout of Magie's 1904
  patent drawing (US Patent 748,626), which is in the public domain (US patents
  published before 1989 carry no copyright unless marked, and this one is not;
  it is also older than 1931). The four boxes in the middle are the patent's
  "Bank", "Wages", "Public treasury" and "Railroad", and Mother Earth's corner
  carries her motto, "Labor upon Mother Earth produces Wages".
- Every **picture** (the globe, Jail, the tree in Central Park, the Poor House,
  the locomotive, the bulb, the trolley, the necessities, the luxury gem, the
  dice, the legacy scroll, the college, the houses and the program icon) is
  original pixel art made for Viola the Landlord, stored as text in
  `src/art.c`. It is released under the GPL with the program and also dedicated
  to the public domain under **CC0 1.0**, so it can be redistributed anywhere.
- The fonts are the X server's own core fonts.

## Sources

- US Patent 748,626, *Game-Board*, Lizzie J. Magie, January 5, 1904
  (public domain).
- US Patent 1,509,312, *Game Board*, Elizabeth Magie Phillips, 1924.
- *The Landlord's Game* rules, Economic Game Co., New York, 1906, as
  transcribed at landlordsgame.info (Thomas Forsyth).
- Wikipedia: *The Landlord's Game*; *Elizabeth Magie*.
- The Public Domain Review: *The Landlord's Game*.
- Mary Pilon, *The Monopolists* (2015).

## Source layout

| File | What it does |
| ---- | ------------ |
| `src/board.c` | the board and the Chance cards, as data |
| `src/game.c` | the rules engine (no X): turns, money, the Single Tax |
| `src/ai.c` | the computer players, Easy / Medium / Hard |
| `src/human.c` | the human players' choices, as dialogs; trades between humans |
| `src/net.c` | network play: localhost listener, ssh-started relay, joiner's ssh pipe, protocol, network seats |
| `src/netui.c` | network play's windows: host setup and waiting room, Join, the joiner's game |
| `src/view.c` | drawing the board and the panel |
| `src/ui.c` | Viola dialogs: messages, lists, check lists, forms, number entry |
| `src/menubar.c` | the Viola menu bar |
| `src/xwin.c` | the only module that talks to X |
| `src/art.c` | the pictures |
| `src/help.c` | the Help texts |
| `src/prefs.c` | the preferences file |
| `src/main.c` | start-up, the main loop, the commands |
| `tests/engine.c` | `make check`: computer-only games and consistency checks |
| `tests/net.c` | `make check`: network games end to end through a stand-in for ssh (table limit, no CPU with four humans, dropped players) |

## License

Copyright (C) 2026 Lee Curtis

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program (`COPYING`). If not, see <https://www.gnu.org/licenses/>.
