# Viola the Landlord - The Landlord's Game for X11
# Copyright (C) 2026 Lee Curtis
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.


# Makefile.am - input for GNU automake; `./autogen.sh` turns it into
# Makefile.in, and `./configure` into the Makefile.
#
#   make            build ./viola-landlord
#   make check      the tests (hundreds of computer-only games, no display needed)
#   make install    into $(prefix)/bin, the man page into $(mandir) (section 6,
#                   games), and the menu entry and icons into $(datadir)
#   make pdf        rebuild docs/viola-landlord-rules.pdf (needs pandoc + pdflatex)
#   make dist       viola-landlord-VERSION.tar.gz, ready to build anywhere
# INSTALL.md describes all of these.

AM_CFLAGS = $(WARN_CFLAGS) $(X11_CFLAGS)

bin_PROGRAMS = viola-landlord

# The rules of the game and the computer players. None of these talk to X,
# so the tests link them without a display.
CORE_SOURCES = \
	src/ai.c \
	src/board.c \
	src/game.c \
	src/net.c \
	src/util.c

viola_landlord_SOURCES = \
	$(CORE_SOURCES) \
	src/art.c \
	src/help.c \
	src/human.c \
	src/main.c \
	src/menubar.c \
	src/netui.c \
	src/prefs.c \
	src/ui.c \
	src/view.c \
	src/xwin.c
viola_landlord_LDADD = $(X11_LIBS)

noinst_HEADERS = \
	src/ai.h \
	src/app.h \
	src/art.h \
	src/board.h \
	src/game.h \
	src/help.h \
	src/human.h \
	src/keys.h \
	src/menubar.h \
	src/net.h \
	src/netui.h \
	src/prefs.h \
	src/ui.h \
	src/util.h \
	src/version.h \
	src/view.h \
	src/xwin.h

man_MANS = viola-landlord.6

EXTRA_DIST = viola-landlord.6.in COPYING autogen.sh .gitignore \
	$(desktop_DATA) $(ICON_PNGS) icons/viola-landlord.svg icons/viola-landlord.xpm \
	$(PDF_SOURCES) docs/images/board-map.svg

# ---- documents ----
# Installed into $(docdir), normally $(prefix)/share/doc/viola-landlord: the
# README, this build guide, the changes, the rules (Markdown and PDF) and the
# pictures the rules show.
dist_doc_DATA = README.md INSTALL.md CHANGELOG.md docs/RULES.md docs/viola-landlord-rules.pdf
docimagesdir = $(docdir)/images
DOC_IMAGES = \
	docs/images/board-map.png \
	docs/images/for-sale.png \
	docs/images/host-not-found.png \
	docs/images/game-board.png \
	docs/images/joiner-view.png \
	docs/images/new-game.png \
	docs/images/start-up.png \
	docs/images/the-deal.png \
	docs/images/waiting-room.png \
	docs/images/which-way.png
dist_docimages_DATA = $(DOC_IMAGES)

# ---- the rules PDF ----
# docs/viola-landlord-rules.pdf ships ready-made. `make pdf` rebuilds it from
# docs/RULES.md (and the board map from src/board.c), when configure found
# pandoc and pdflatex. The PDF is written into the source tree, so it is
# shipped and installed whether or not it was rebuilt.
PDF_SOURCES = docs/rules-pdf.yaml docs/rules-pdf.lua docs/rules-header.tex docs/boardmap.c
EXTRA_PROGRAMS = docs/boardmap
docs_boardmap_SOURCES = docs/boardmap.c src/board.c
docs_boardmap_CFLAGS = $(WARN_CFLAGS) -I$(top_srcdir)/src
CLEANFILES = docs/boardmap$(EXEEXT)

if HAVE_PANDOC
pdf: docs/boardmap$(EXEEXT)
	./docs/boardmap$(EXEEXT) > "$(srcdir)/docs/images/board-map.svg"
	@if command -v rsvg-convert >/dev/null 2>&1; then \
	    echo "  rsvg-convert docs/images/board-map.png"; \
	    rsvg-convert -w 1600 "$(srcdir)/docs/images/board-map.svg" -o "$(srcdir)/docs/images/board-map.png"; \
	else echo "(rsvg-convert not found: docs/images/board-map.png left as it is)"; fi
	cd "$(srcdir)/docs" && $(PANDOC) --defaults=rules-pdf.yaml --lua-filter=rules-pdf.lua \
	    -H rules-header.tex --pdf-engine="$(PDFLATEX)" --resource-path=. RULES.md -o viola-landlord-rules.pdf
	@echo "Made docs/viola-landlord-rules.pdf"
else
pdf:
	@echo "make pdf needs pandoc and pdflatex, which configure did not find."
	@echo "The shipped docs/viola-landlord-rules.pdf is up to date with this release."
endif
.PHONY: pdf

# ---- desktop ----
# The menu entry and the icon (a house on a lot over a title deed, pixel art
# in the Viola palette) at every size, where the freedesktop.org standards
# say; plus the classic XPM in $(datadir)/pixmaps for older window managers.
desktopdir = $(datadir)/applications
desktop_DATA = viola-landlord.desktop
icondir = $(datadir)/icons/hicolor
ICON_SIZES = 16 22 24 32 48 64 128 256
ICON_PNGS = $(ICON_SIZES:%=icons/viola-landlord-%.png)

install-data-local:
	@for s in $(ICON_SIZES); do \
	    $(MKDIR_P) "$(DESTDIR)$(icondir)/$${s}x$${s}/apps" && \
	    $(INSTALL_DATA) "$(srcdir)/icons/viola-landlord-$$s.png" "$(DESTDIR)$(icondir)/$${s}x$${s}/apps/viola-landlord.png" || exit 1; \
	done
	$(MKDIR_P) "$(DESTDIR)$(icondir)/scalable/apps" "$(DESTDIR)$(datadir)/pixmaps"
	$(INSTALL_DATA) "$(srcdir)/icons/viola-landlord.svg" "$(DESTDIR)$(icondir)/scalable/apps/viola-landlord.svg"
	$(INSTALL_DATA) "$(srcdir)/icons/viola-landlord.xpm" "$(DESTDIR)$(datadir)/pixmaps/viola-landlord.xpm"

uninstall-local:
	@for s in $(ICON_SIZES); do rm -f "$(DESTDIR)$(icondir)/$${s}x$${s}/apps/viola-landlord.png"; done
	rm -f "$(DESTDIR)$(icondir)/scalable/apps/viola-landlord.svg" "$(DESTDIR)$(datadir)/pixmaps/viola-landlord.xpm"

install-data-hook:
	@if test -z "$(DESTDIR)"; then \
	    if test -f "$(icondir)/index.theme"; then \
	        gtk-update-icon-cache -q -f "$(icondir)" 2>/dev/null || :; fi; \
	    if test -f "$(desktopdir)/mimeinfo.cache"; then \
	        update-desktop-database -q "$(desktopdir)" 2>/dev/null || :; fi; \
	fi

# ---- tests ----
# tests/engine plays hundreds of games between the computer players and
# checks the rules engine stays consistent and that the levels are in order.
# tests/net checks the network protocol's message format and that seats
# left for absent network players are played by the computer.
check_PROGRAMS = tests/engine tests/net
TESTS = tests/engine tests/net
tests_engine_SOURCES = tests/engine.c $(CORE_SOURCES)
tests_engine_CFLAGS = $(WARN_CFLAGS) -I$(top_srcdir)/src
tests_net_SOURCES = tests/net.c $(CORE_SOURCES)
tests_net_CFLAGS = $(WARN_CFLAGS) -I$(top_srcdir)/src

test: check
