#!/bin/sh
# XLandlord - The Landlord's Game for X11
# Copyright (C) 2026 Lee Curtis
# SPDX-License-Identifier: GPL-3.0-or-later
#
# tests/server.sh - `make check` for XLandlord --server: it starts with no
# X display, opens its table, writes its log in ~/log/, says so on the
# console, and stops cleanly on SIGTERM. (Whole network games are played
# by tests/net.)

set -u
prog=${XLANDLORD:-./XLandlord}
home=$(mktemp -d) || exit 1
out="$home/console.txt"
trap 'rm -rf "$home"' EXIT
port=$((22000 + $$ % 20000))

env -u DISPLAY HOME="$home" "$prog" --server --port "$port" --start-wait 5 --name "Test server" >"$out" 2>&1 &
pid=$!
for i in 1 2 3 4 5 6 7 8 9 10; do
    grep -q "Waiting for players on port $port" "$out" 2>/dev/null && break
    sleep 1
done
kill -TERM "$pid" 2>/dev/null
for i in 1 2 3 4 5; do kill -0 "$pid" 2>/dev/null || break; sleep 1; done

fail=0
check() { if ! eval "$1"; then echo "FAIL: $2"; fail=1; fi; }
check "! kill -0 $pid 2>/dev/null" "the server did not stop on SIGTERM"
check "grep -q 'Waiting for players on port $port (hosted by Test server' '$out'" "no waiting line on the console"
check "grep -q 'The server has stopped.' '$out'" "no stopping line on the console"
log=$(ls "$home"/log/xlandlord-*.log 2>/dev/null | head -1)
check "[ -n '$log' ]" "no log file in ~/log/"
[ -n "$log" ] && check "grep -q 'Waiting for players on port $port' '$log'" "the log does not have the console's lines"
[ "$fail" = 0 ] || { echo "--- console:"; cat "$out"; }
kill -KILL "$pid" 2>/dev/null
exit $fail
