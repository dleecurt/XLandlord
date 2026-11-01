/*
 * XLandlord - The Landlord's Game for X11
 * Copyright (C) 2026 Lee Curtis
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
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

/* version.h - the version number, from configure (AC_INIT in configure.ac). */
#ifndef VERSION_H
#define VERSION_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef PACKAGE_VERSION
#define PACKAGE_VERSION_STR PACKAGE_VERSION
#else
#define PACKAGE_VERSION_STR "0.0.8"
#endif

#endif
