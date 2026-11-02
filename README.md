# XLandlord

**The Landlord's Game**, the board game Lizzie J. Magie patented in 1904
(US Patent 748,626) and the ancestor of Monopoly, for X11, played against the
computer. It follows the rules published by the Economic Game Company of New
York in 1906, **including the Single Tax rules**, and is written in C with
plain Xlib. The window is drawn in the style of the standalone **Viola** web
browser, like Ted Viola: flat gray94 panels, 2px light and dark bevels, 1px
black boxes, Helvetica menus with Viola's small triangles, black focus
rectangles on the buttons.

The game and the program you run are both called **XLandlord** (up to
version 0.0.6 it was called Viola the Landlord). The source is at
<https://github.com/dleecurt/XLandlord>.

XLandlord is free software, released under the **GNU General Public
License, version 3 or (at your option) any later version**. See
[License](#license).

## Quick start

From a release tarball (`xlandlord-X.Y.Z.tar.gz`) you need a C compiler,
make and the X11 development files:

```
./configure     # checks the system (GNU autoconf); --prefix=DIR chooses where to install
make            # builds ./XLandlord
make check      # plays hundreds of computer-only games to test the rules (no display needed)
make install    # into /usr/local by default (needs privileges); man page:  man 6 XLandlord
make uninstall
make clean

./XLandlord
./XLandlord --version
```

From a source checkout (no `configure` yet), run `./autogen.sh` first; it
needs GNU autoconf, automake and pkg-config.

**[INSTALL.md](INSTALL.md)** is the full guide to building and installing:
packages for each system, configure options, every make target, what is
installed where, uninstalling, network-play requirements and troubleshooting.

**[docs/RULES.md](docs/RULES.md)** is the complete rule book and how-to-play
guide, with screenshots, a numbered board map and a quick-reference page; the
same document is in **[docs/xlandlord-rules.pdf](docs/xlandlord-rules.pdf)**
for printing. Both are installed in `$(prefix)/share/doc/xlandlord`. The
PDF (20 pages, its last a one-page quick reference) is the one to print:
`xdg-open /usr/local/share/doc/xlandlord/xlandlord-rules.pdf`, or
`lp` the same file. `make pdf` rebuilds it after editing `docs/RULES.md`
(see [INSTALL.md](INSTALL.md), section 9). Help > The Rules of 1906 and
`man 6 XLandlord` say where it is installed.

**The manual page**, `man 6 XLandlord`, is installed with the game. It holds
everything in one place: the command line, building and installing, setting
up password-less ssh for network play, every preferences key, and the full
rules of the game.

**In the game**, Help > How to Play (F1) is an illustrated guide: twenty
pages, each with a screenshot and a short explanation, from starting a game
through bidding, Jail and the Single Tax to network play. Back / Next (or the
arrow keys) turn the pages.

| System        | Build                           | Fonts (to look like Viola)                  |
| ------------- | ------------------------------- | ------------------------------------------- |
| Debian/Ubuntu | `sudo apt install libx11-dev`   | `sudo apt install xfonts-base xfonts-75dpi` |
| Fedora/RHEL   | `sudo dnf install libX11-devel` | `xorg-x11-fonts-misc xorg-x11-fonts-75dpi`  |
| NetBSD        | the X sets (xbase, xcomp)       | the X font set (xfont)                      |

Without the 75dpi fonts the game still runs; X picks the nearest fonts it has.

Current version: **0.0.15** (see `CHANGELOG.md`).

## Playing

The game opens on a **title card** that asks how you want to play: **Host a
Network Game**, **Join a Network Game**, **Play Here** or **Quit**. Then it
asks **your name**: the name you play under, shown to the other players and
used in network games. It is never taken from your login name; the one used
last time is filled in, so Enter keeps it (Cancel goes back to the title
card).

A game here starts with the **New Game** form: four seats, each **Empty,
Human or Computer** (two to four players, at least one human, and **one
computer player at most**), the **computer skill** for the game (Easy,
Medium or Hard), how
many times a player draws his wages before the game ends (the 1906 rules say
five) and the rules (standard, or the 1906 "advanced and scientific" rules).
Seat 1 is you; each other human seat is asked for its player's name. Then
the cards are dealt
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

**Starting up.** The title card asks how you want to play - **Host a
Network Game**, **Join a Network Game**, or **Play Here** (at this screen,
against the computer or hot-seat) - and then your name. The same choices
are in the Game menu at any time. To skip the question, start it with an
option (the title card then shows for two seconds, any key skipping it):

```
XLandlord --host                 # host a network game
XLandlord --join 192.168.1.20    # join the game on that machine
XLandlord --join lee@gamebox     # ... by host name, as user lee
XLandlord --local                # play at this screen
```

**Hosting.** Game > Host a Network Game. First the host chooses **Host and
Play** (he takes a seat, and up to three others join) or **Host Only** (he
only runs the game and watches the board on his screen; all four seats are
for joiners, and he has no turn, though he can still remove a silent player
or end the game). Either way he sets the game up for everyone: the computer
player's skill, the length of the game, the rules,
how long to wait for a player whose connection drops, and how long players
have to answer (both 1, 2 or 5 minutes), then a port (1904 by default; it is
only reachable from the host's own machine). A waiting room lists who has
joined; press **Start Game** when everyone is in (at least one player must
have joined; with one, he plays the computer player).

**Reaching the host.** The waiting room shows how to reach this machine: its host name
and its IP addresses (loopback and link-local addresses are left out), for
example `lee@gamebox (or the IP address: 192.168.1.20 / 2001:db8::5)`.

**Who is where.** Each player's machine is shown next to his name, in both
waiting rooms and in the player boxes: the host's own host name, and for a
joiner the name of the machine his ssh connection comes from (the relay
passes on ssh's `SSH_CONNECTION` client address and the host looks it up in
its hosts file and DNS). A machine with no name is shown by its IP address,
e.g. `Ann (from annbox)`, `Bea (from 203.0.113.45)`.

**Joining.** Join a Network Game, and enter the host's address. You see the
same board and the same dialogs as at the host; on your turn the Roll / Build /
Trade buttons work. The menus and dialogs say whose turn it is.

**Names.** Everyone is known at the table by the name he gave at start-up,
not by his login name (which only ssh uses, to log in to the host). No two
players may share a name, whatever the letter case: a joiner whose name is
taken - by the host or by someone already waiting - is told so and asked for
another, and his game joins again with it.

**Giving the host's address.** Type the host's **IP address** (IPv4 such as
`192.168.1.20`, or IPv6 such as `2001:db8::5`) or its **host name** (such as
`gamebox` or `gamebox.example.org`). A name is looked up the way the system
looks up every name: in the machine's **hosts file** (`/etc/hosts`) and in
**DNS**, in the order `/etc/nsswitch.conf` gives. If the name is in neither, the
game says so (*Host Not Found*) before trying ssh, and you can type the IP
address instead or add a line such as

```
192.168.1.20   gamebox
```

to `/etc/hosts` (as root). Add `user@` in front to log in to the host as
another user, and `:port` after it if the host chose another port than 1904;
for an IPv6 address with a port, put the address in brackets:
`lee@[2001:db8::5]:1905`. ssh is given the name as you typed it, so its
`known_hosts` entry for that name keeps working.

**The table.**

- Four players at most: the host and three joiners, or four joiners when the
  host only hosts. A fifth player who tries to
  join is turned away with a warning ("The table is full..."); so is anyone who
  tries to join a game that has already started (except a player coming back
  after a dropped connection).
- If fewer than four humans play, **one** computer player takes a free seat,
  at the skill the host chose; joiners have no say in it. With four humans
  there is no computer player: a fourth human who joins before the start
  takes its seat.
- Once the game has started nobody new may join, and the computer player
  stays to the end. Starting money is set once, by the 1906 table, for the
  players seated at the start.
- **Quitting.** A human who quits is out: his cards go back into the pack,
  his houses come down, his cash goes with him, and nobody takes his seat.
  With one player left the game is over.
- **A dropped connection.** His seat waits for him for the host's *Wait for a
  dropped player* time (1, 2 or 5 minutes). His game reconnects by itself
  every few seconds; the same name from the same machine gets the seat back
  and is asked his question again. Play pauses whenever it needs him, with a
  countdown on every screen. Not back in time: he leaves the game.
- **Taking too long.** Each question, and each turn, has the host's *Time to
  answer* (1, 2 or 5 minutes). Then the player's game asks "Are you still
  there?" for one more minute; if he does not answer, he is taken out so
  play can go on, and told why. The host may also remove a player he is
  waiting for.
- **The host.** If the host leaves, the game is over for everyone, and each
  joiner is told so - also when the host's game simply stops.

**How it works.** The joiner's program runs

```
ssh -T -o BatchMode=yes user@host XLandlord --relay 1904
```

On the host, ssh starts a second copy of XLandlord as a *relay*, which
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
   ssh -o BatchMode=yes user@host XLandlord --version
   ```

If step 4 fails:

- "Permission denied": the key is not in the host's
  `~/.ssh/authorized_keys`, or the permissions are too open (`chmod 700 ~/.ssh`
  and `chmod 600 ~/.ssh/authorized_keys` on the host).
- "Host key verification failed": do the plain `ssh user@host` once.
- "command not found": non-interactive ssh does not see the host's PATH (for
  instance `/usr/local/bin`). Either fix the PATH for non-interactive shells on
  the host, or set `net_command = /usr/local/bin/XLandlord` (the full path
  on the host) in the joiner's `~/.config/xlandlord/preferences.conf`.
- Every machine needs the **same version** of XLandlord; a different
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

### The computer player

There is one computer player at most, and its skill is set for the game by
whoever sets the game up (the host, in a network game):

| Level  | How it plays |
| ------ | ------------ |
| Easy   | On impulse: buys about half of what it can afford, almost never bids or builds, picks moves at random, keeps little cash. |
| Medium | By rules of thumb: buys and builds while it keeps a $100 cushion, bids a little over the price for good cards, pays out of Jail early in the game, avoids an obviously expensive square. |
| Hard   | By counting: values each card at 100 plus the rent it should bring in the circuits left, keeps in hand what the trip round the board is likely to cost, hunts railroad monopolies and the Municipal Cinch (and blocks yours), bids up to a card's worth, sits in Jail late in the game when the board is dangerous, picks the cheapest square for a pass or a backward move, and votes on the Single Tax by what it does to its standing. |

`make check` plays the levels against each other (the rules engine can
seat several; the game never does). Over the test's games Hard beats
Easy about 96% of the time, Medium beats Easy about 86%, and Hard beats Medium
about 59% (the dice still have their say).

### Options

Options > Game Settings (also your name), Computer Speed (fast, normal,
slow), **Ask Me to Bid** (off: you are never asked to bid when a computer
player lands on an unowned card), **Confirm Each Computer Move** (wait for a key
after every computer turn). They are kept in
`~/.config/xlandlord/preferences.conf` (`$XDG_CONFIG_HOME` is honored),
with the seats as `seat1` .. `seat4` (`empty`, `human`, `computer`; one
`computer` at most), the computer player's skill as `cpu_skill` (`easy`,
`medium`, `hard`; also used for hosted games) and the human names as
`name1` .. `name4`; for network play
`net_host` (the last host joined), `net_port` (the port to host on),
`net_command` (the program ssh runs on the host, normally `XLandlord`),
`net_reconnect` and `net_answer` (the hosted game's timers, in minutes: 1,
2 or 5), and `net_host_only` (1: host without playing).
Older files are converted (a level per computer seat and `net_cpu` become
one computer seat and `cpu_skill`); a 0.0.1 file is converted too,
and a file from before the game was renamed XLandlord
(`~/.config/viola-landlord/preferences.conf`) is read once and saved under
the new name.

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
  original pixel art made for XLandlord, stored as text in
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
| `src/ai.c` | the computer player's three levels, Easy / Medium / Hard |
| `src/human.c` | the human players' choices, as dialogs; trades between humans |
| `src/net.c` | network play: localhost listener, ssh-started relay, joiner's ssh pipe, protocol, network seats, the reconnect and answer timers |
| `src/netui.c` | network play's windows: host setup, waiting rooms, the host's waiting panel, Join, the joiner's game, "Are you still there?", reconnecting |
| `src/view.c` | drawing the board and the panel |
| `src/ui.c` | Viola dialogs: messages, lists, check lists, forms, number entry |
| `src/menubar.c` | the Viola menu bar |
| `src/xwin.c` | the only module that talks to X |
| `src/art.c` | the pictures |
| `src/help.c` | the Help texts |
| `src/guide.c` | Help > How to Play, the illustrated guide |
| `src/guidepics.c` | its screenshots, compiled in (made by `make guide-pics`) |
| `src/prefs.c` | the preferences file |
| `src/splash.c` | the title card at start-up, with the first question |
| `src/main.c` | start-up (how to play, then the player's name), the main loop, the commands |
| `XLandlord.6.in` | the manual page |
| `docs/RULES.md` | the rule book and how-to-play guide (source of the PDF) |
| `docs/boardmap.c` | writes the numbered board map for the rules from `src/board.c` |
| `docs/guide/*.png` | the guide's screenshots, the source of `src/guidepics.c` |
| `docs/guidepics.c` | turns them into `src/guidepics.c` |
| `tests/engine.c` | `make check`: computer-only games, consistency checks, a player leaving mid-game |
| `tests/net.c` | `make check`: network games end to end through a stand-in for ssh (table limit, no computer player with four humans, quitting, reconnecting, a player who stops answering, names taken, host-only games) |

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
