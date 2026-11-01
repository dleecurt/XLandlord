#!/bin/sh
# XLandlord - The Landlord's Game for X11
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

# autogen.sh - make the build files that a source checkout does not contain.
#
# A release tarball (xlandlord-X.Y.Z.tar.gz) already holds the generated
# `configure` script and Makefile.in, so people who only want to BUILD the
# program never need this. You need it when you work from the source tree
# itself, or after editing configure.ac or Makefile.am (make re-runs the tools
# by itself in that case, once they have been run here the first time).
#
# Requires GNU autoconf (2.69 or newer) and GNU automake (1.11 or newer).
set -e
cd "$(dirname "$0")"
autoreconf --install --verbose
echo
echo "Now run:  ./configure && make && make check"
