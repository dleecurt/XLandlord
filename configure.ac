dnl XLandlord - The Landlord's Game for X11
dnl Copyright (C) 2026 Lee Curtis
dnl
dnl SPDX-License-Identifier: GPL-3.0-or-later
dnl
dnl This program is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <https://www.gnu.org/licenses/>.


dnl configure.ac - input for GNU autoconf; `./autogen.sh` turns it into the
dnl `configure` script. It checks for a C compiler and the X11 library.
dnl
dnl THE VERSION NUMBER lives here, in AC_INIT. It reaches the program through
dnl config.h (PACKAGE_VERSION, see src/version.h). It starts at 0.0.1 and goes
dnl up by one with every release.

AC_PREREQ([2.69])
AC_INIT([XLandlord], [0.0.14], [], [xlandlord])
AC_COPYRIGHT([Copyright (C) 2026 Lee Curtis
License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects tar-ustar -Wall -Wno-portability])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AS_IF([test "x$ac_cv_prog_cc_c99" = xno],
  [AC_MSG_ERROR([a C99 compiler is required (try CC="gcc -std=gnu99")])])

dnl Warnings, if the compiler takes them.
AC_MSG_CHECKING([whether $CC accepts -Wall -Wextra])
save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Wall -Wextra"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
  [WARN_CFLAGS="-Wall -Wextra"; AC_MSG_RESULT([yes])],
  [WARN_CFLAGS=; AC_MSG_RESULT([no])])
CFLAGS=$save_CFLAGS
AC_SUBST([WARN_CFLAGS])

dnl The timed wait for the computer players' moves.
AC_CHECK_HEADERS([sys/select.h sys/time.h], [],
  [AC_MSG_ERROR([the system lacks $ac_header, which XLandlord needs])])
AC_CHECK_FUNCS([select gettimeofday], [],
  [AC_MSG_ERROR([the C library lacks $ac_func, which XLandlord needs])])

dnl ---- X11 ----
dnl XLandlord draws everything itself with plain Xlib, like the Viola
dnl browser: no toolkit. pkg-config finds libX11 on any modern system; without
dnl pkg-config we try the usual -lX11 (AC_PATH_XTRA's places included).
PKG_PROG_PKG_CONFIG
AS_IF([test -n "$PKG_CONFIG"],
  [PKG_CHECK_MODULES([X11], [x11], [have_x11=yes], [have_x11=no])],
  [have_x11=no])
AS_IF([test "x$have_x11" != xyes], [
  AC_PATH_XTRA
  save_CPPFLAGS=$CPPFLAGS; save_LIBS=$LIBS
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"
  LIBS="$X_LIBS -lX11 $LIBS"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <X11/Xlib.h>]], [[return XOpenDisplay(0) != 0;]])],
    [have_x11=yes; X11_CFLAGS=$X_CFLAGS; X11_LIBS="$X_LIBS -lX11"])
  CPPFLAGS=$save_CPPFLAGS; LIBS=$save_LIBS
])
AS_IF([test "x$have_x11" != xyes],
  [AC_MSG_ERROR([the X11 library (libX11) was not found. Install its development package:
  Debian/Ubuntu:  sudo apt install libx11-dev
  Fedora/RHEL:    sudo dnf install libX11-devel
  Arch:           sudo pacman -S libx11
  NetBSD:         it comes with the X sets (xbase, xcomp)])])
AC_SUBST([X11_CFLAGS])
AC_SUBST([X11_LIBS])

dnl ---- the rules PDF (optional) ----
dnl docs/xlandlord-rules.pdf ships ready-made; `make pdf` rebuilds it
dnl from docs/RULES.md with pandoc and a LaTeX engine, when both are here.
AC_ARG_VAR([PANDOC], [pandoc, to rebuild the rules PDF with make pdf])
AC_ARG_VAR([PDFLATEX], [the LaTeX engine pandoc uses for the rules PDF])
AC_CHECK_PROGS([PANDOC], [pandoc])
AC_CHECK_PROGS([PDFLATEX], [pdflatex])
AS_IF([test -n "$PANDOC" && test -n "$PDFLATEX"],
  [pdf_note="make pdf rebuilds it with $PANDOC"],
  [pdf_note="shipped copy (pandoc or pdflatex not found, so make pdf is off)"])
AM_CONDITIONAL([HAVE_PANDOC], [test -n "$PANDOC" && test -n "$PDFLATEX"])

AC_CONFIG_FILES([Makefile XLandlord.6])
AC_OUTPUT

AC_MSG_NOTICE([
  XLandlord $VERSION is configured:
    compiler:        $CC
    X11:             $X11_CFLAGS $X11_LIBS
    install prefix:  $prefix
    rules PDF:       $pdf_note
  Next:  make   make check   make install])
