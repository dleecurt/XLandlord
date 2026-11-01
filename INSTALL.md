# Building and installing Viola the Landlord

This guide covers building Viola the Landlord from source, installing and
uninstalling it, and the problems you are most likely to run into. For what
the game is and how to play it, see `README.md` and `docs/RULES.md` (or its
PDF, `docs/viola-landlord-rules.pdf`).

Contents:

1. [What you need](#1-what-you-need)
2. [Building from a release tarball](#2-building-from-a-release-tarball)
3. [Building from a git checkout](#3-building-from-a-git-checkout)
4. [Configure options](#4-configure-options)
5. [Make targets](#5-make-targets)
6. [What gets installed, and where](#6-what-gets-installed-and-where)
7. [Uninstalling](#7-uninstalling)
8. [Network play: what each machine needs](#8-network-play-what-each-machine-needs)
9. [Rebuilding the rules PDF](#9-rebuilding-the-rules-pdf)
10. [Troubleshooting](#10-troubleshooting)

## 1. What you need

To **build** the game:

- a C99 compiler (gcc or clang)
- make
- the X11 client library and its headers (libX11)
- pkg-config (recommended; configure falls back to the usual places without it)

To **run** it well:

- an X server (any X11 desktop, or `Xvfb` for testing)
- the 75dpi bitmap fonts, so the window looks like the Viola browser:
  Adobe Helvetica for menus and dialogs, misc-fixed for the record of events.
  Without them the game still runs, with the nearest fonts the server has.

To build from a **git checkout** you also need GNU autoconf (2.69 or newer)
and GNU automake (1.11 or newer). To **rebuild the rules PDF** (optional; a
ready-made copy ships with the source) you need pandoc and a LaTeX engine.

The packages, by system:

| System        | To build                                         | Fonts                                         |
| ------------- | ------------------------------------------------ | --------------------------------------------- |
| Debian/Ubuntu | `sudo apt install build-essential libx11-dev pkg-config` | `sudo apt install xfonts-base xfonts-75dpi` |
| Fedora/RHEL   | `sudo dnf install gcc make libX11-devel pkgconf` | `sudo dnf install xorg-x11-fonts-misc xorg-x11-fonts-75dpi` |
| Arch          | `sudo pacman -S base-devel libx11 pkgconf`       | `sudo pacman -S xorg-fonts-75dpi xorg-fonts-misc` |
| NetBSD        | the base system and the X sets (xbase, xcomp); `pkgin install pkgconf` | the X font set (xfont) |

From a git checkout add autoconf and automake:

| System        | Autotools                                     |
| ------------- | --------------------------------------------- |
| Debian/Ubuntu | `sudo apt install autoconf automake`          |
| Fedora/RHEL   | `sudo dnf install autoconf automake`          |
| Arch          | included in `base-devel`                      |
| NetBSD        | `pkgin install autoconf automake`             |

After installing new X fonts, either restart the X server or run
`xset fp rehash`.

## 2. Building from a release tarball

A release tarball (`viola-landlord-X.Y.Z.tar.gz`) already contains the
`configure` script, so you only need the build tools from section 1.

```
tar xzf viola-landlord-0.0.6.tar.gz
cd viola-landlord-0.0.6
./configure
make
make check            # optional, about 10 seconds, needs no display
sudo make install
```

Then start it from your desktop's Games menu, or from a terminal:

```
viola-landlord
```

`configure` ends with a summary, for example:

```
  Viola the Landlord 0.0.6 is configured:
    compiler:        gcc
    X11:             -lX11
    install prefix:  /usr/local
    rules PDF:       shipped copy (pandoc or pdflatex not found, so make pdf is off)
```

You can also run the game without installing it: `./viola-landlord` in the
build directory works the same.

## 3. Building from a git checkout

A checkout does not contain the generated files (`configure`, `Makefile.in`,
`config.h.in`). Make them once with `autogen.sh`:

```
git clone <the repository's URL> viola-landlord
cd viola-landlord
./autogen.sh          # runs autoreconf --install
./configure
make
make check
sudo make install
```

After that, `make` re-runs the autotools by itself whenever you edit
`configure.ac` or `Makefile.am`.

**Building in a separate directory.** Like any Automake project the build can
live outside the source tree, which keeps the source clean:

```
mkdir build && cd build
../configure
make
```

## 4. Configure options

The usual GNU options all work; these are the ones that matter here:

| Option | Effect |
| ------ | ------ |
| `--prefix=DIR` | Install under `DIR` instead of `/usr/local`. `--prefix=$HOME/.local` installs for yourself only, without root. |
| `--bindir=DIR` | Where the program goes (default `PREFIX/bin`). |
| `--mandir=DIR` | Where the man page goes (default `PREFIX/share/man`; it is installed in section 6, games). |
| `--docdir=DIR` | Where the documents go (default `PREFIX/share/doc/viola-landlord`). |
| `CC=clang` | Use another compiler. |
| `CFLAGS="-O2 -g"` | Compiler flags (the default is `-g -O2`; `-Wall -Wextra` are added when the compiler takes them). |
| `PKG_CONFIG_PATH=...` | Where pkg-config looks for `x11.pc`, for an X11 in an unusual place. |
| `X11_CFLAGS=... X11_LIBS=...` | Give the X11 flags by hand and skip the search. |
| `PANDOC=...`, `PDFLATEX=...` | The pandoc and LaTeX engine `make pdf` uses (see section 9). |

`./configure --help` lists everything.

**Staged installs for packaging.** `DESTDIR` works as usual:

```
make install DESTDIR=/tmp/stage
```

With `DESTDIR` set, the icon and menu caches are not touched (the package
manager does that).

## 5. Make targets

| Target | What it does |
| ------ | ------------ |
| `make` | Builds `./viola-landlord`. |
| `make check` | Runs the tests (also `make test`). `tests/engine` plays several hundred games between computer players and checks the rules engine and that Hard beats Medium beats Easy; `tests/net` plays whole network games through a stand-in for ssh (the four-player limit, no computer player with four humans, a player dropping out). No display is needed. |
| `make install` | Installs everything listed in section 6. |
| `make uninstall` | Removes it again. |
| `make pdf` | Rebuilds `docs/viola-landlord-rules.pdf` from `docs/RULES.md` (needs pandoc and LaTeX; see section 9). |
| `make clean` | Removes what `make` built. |
| `make distclean` | Also removes what `configure` made. |
| `make dist` | Makes `viola-landlord-X.Y.Z.tar.gz`, ready to build anywhere. |
| `make distcheck` | Makes the tarball, then unpacks, builds, tests and installs it in a scratch directory to prove it is complete. |

## 6. What gets installed, and where

With the default prefix `/usr/local`:

| File | Where |
| ---- | ----- |
| the program | `/usr/local/bin/viola-landlord` |
| the man page | `/usr/local/share/man/man6/viola-landlord.6` (`man 6 viola-landlord`) |
| the menu entry | `/usr/local/share/applications/viola-landlord.desktop` |
| the icon, at 16 to 256 pixels | `/usr/local/share/icons/hicolor/NxN/apps/viola-landlord.png` |
| the scalable icon | `/usr/local/share/icons/hicolor/scalable/apps/viola-landlord.svg` |
| the classic X icon | `/usr/local/share/pixmaps/viola-landlord.xpm` |
| the documents | `/usr/local/share/doc/viola-landlord/`: `README.md`, `INSTALL.md`, `CHANGELOG.md`, `RULES.md`, `viola-landlord-rules.pdf` and the board pictures the rules use |

The game keeps no files of its own outside your home directory. Its settings
live in `~/.config/viola-landlord/preferences.conf` (or
`$XDG_CONFIG_HOME/viola-landlord/preferences.conf`), written the first time you
change something.

## 7. Uninstalling

From the same build directory, with the same `configure` options:

```
sudo make uninstall
```

This removes every file in section 6 (empty directories are left). Your
settings stay; remove them with:

```
rm -r ~/.config/viola-landlord
```

## 8. Network play: what each machine needs

Network play needs nothing extra at build time: it uses plain sockets and runs
the system's `ssh` program. At run time:

- **Every machine** needs the same version of Viola the Landlord installed (a
  different version is turned away with a message).
- **The host** needs an ssh server running (`sudo apt install openssh-server`,
  or `sshd=YES` in NetBSD's `/etc/rc.conf`). The game listens on
  `127.0.0.1` only (port 1904 by default), so no firewall port needs opening.
- **Each joiner** must be able to reach the host by its **IP address** or by
  a **host name** that its machine can look up: in DNS, or in its own
  `/etc/hosts` (a line such as `192.168.1.20   gamebox`). The game looks the
  name up with the system resolver (`getaddrinfo`, which follows
  `/etc/nsswitch.conf`) and says plainly when a name is not found. The host's
  waiting room lists its name and IP addresses.
- **Each joiner** needs the ssh client and password-less (key) access to the
  host. `README.md` ("Setting up password-less ssh") and Help > Network Play
  walk through it; the check is:

  ```
  ssh -o BatchMode=yes user@host viola-landlord --version
  ```

  It must print the version without asking for anything.
- If the host installed the game outside the PATH that non-interactive ssh
  sees (for instance under `--prefix=$HOME/.local`), set the full path on each
  joiner's machine in `~/.config/viola-landlord/preferences.conf`:

  ```
  net_command = /home/lee/.local/bin/viola-landlord
  ```

## 9. Rebuilding the rules PDF

`docs/viola-landlord-rules.pdf` is generated from `docs/RULES.md` and comes
ready-made with the source, so you never need to build it to play or install.
To rebuild it after editing the rules:

```
make pdf
```

This needs **pandoc** and a **LaTeX engine** with the usual packages
(Debian/Ubuntu: `sudo apt install pandoc texlive-latex-recommended
texlive-fonts-recommended texlive-latex-extra`). `configure` looks for them; if
they are missing, `make pdf` says so and the shipped PDF is used as it is.

`make pdf` first regenerates the board map from the game's own board data
(`docs/boardmap.c` reads `src/board.c` and writes `docs/images/board-map.svg`;
`rsvg-convert`, from librsvg, makes the PNG the PDF uses, and is skipped with a
note if missing). The screenshots in `docs/images/` are part of the source: if
you change the look of the window, take new ones and replace them.

The PDF's look is set by three small files next to `RULES.md`:
`rules-pdf.yaml` (pandoc's settings: paper, fonts, contents),
`rules-header.tex` (LaTeX styling) and `rules-pdf.lua` (a pandoc filter that
sizes the pictures, drops the hand-written contents list in favour of the PDF's
own, and puts the quick reference on a page of its own). `RULES.md` itself stays
plain Markdown that reads well anywhere.

## 10. Troubleshooting

**`configure: error: the X11 library (libX11) was not found`**
Install the X11 development package (section 1). If X11 is somewhere unusual,
point pkg-config at it (`PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig ./configure`)
or give `X11_CFLAGS` and `X11_LIBS` yourself.

**`./autogen.sh: autoreconf: not found`, or `possibly undefined macro: PKG_PROG_PKG_CONFIG`**
Install autoconf, automake and pkg-config (the last provides the `PKG_*`
macros), then run `./autogen.sh` again.

**`viola-landlord: cannot open the X display`**
The game is an X11 program: start it inside an X session, or set `DISPLAY`
(for example `DISPLAY=:0`). Over ssh use `ssh -X`.

**`viola-landlord: no usable X font found`**
The X server has no fonts at all that the game can use. Install
`xfonts-base` (or your system's equivalent) and run `xset fp rehash`.

**The window works but does not look like Viola** (wrong typeface, uneven
text): the 75dpi Adobe fonts are missing. Install them (section 1) and restart
the game. `xlsfonts | grep adobe-helvetica` should list them.

**The menu entry or icon does not appear after `make install`**
Some desktops cache them. Log out and in again, or run
`update-desktop-database ~/.local/share/applications` and
`gtk-update-icon-cache -f /usr/local/share/icons/hicolor` (the install does
this itself when those caches already exist).

**`make check` fails in `tests/net`**
The test opens a port on `127.0.0.1` between 20000 and 40000 and runs copies
of itself as joiners. A sandbox that forbids local sockets or `fork()` will
make it fail; run it outside the sandbox. Its output names the check that
failed.

**Network play: "Host Not Found"**
The name typed is in neither DNS nor `/etc/hosts` on the joiner's machine.
Type the host's IP address instead (the host's waiting room shows it), or add
the name to `/etc/hosts`. `getent hosts NAME` shows what the system finds.

**Network play: "Could not join ... Permission denied (publickey)"** and
other ssh messages: ssh's own error is shown as it is; see section 8 and the
ssh part of `README.md`. "no game is being hosted on this machine" means ssh
worked but the host has not opened a game (Game > Host a Network Game) on that
port.
