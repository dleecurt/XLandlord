/*
 * Viola the Landlord - The Landlord's Game for X11
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

/* help.c - see help.h. */
#include <stddef.h>

#include "help.h"

const char *const help_play[] = {
    "THE OBJECT",
    "  Get as much wealth as possible. When one player has drawn his wages",
    "  five times (Options > Game Settings changes the number) the game ends",
    "  and the richest player wins: cash, plus 100 for every card, every",
    "  house and every luxury held.",
    "",
    "A TURN",
    "  Press Roll Dice (Space). Your checker moves; the space you stop on",
    "  is settled by the rules, and a dialog asks whenever you have a choice.",
    "  Before you roll you may build houses, buy or sell cards, or propose",
    "  the Single Tax (the Game menu, or the B, T and S keys).",
    "",
    "THE SPACES",
    "  Green lots       buy them; others pay you land rent (+$10 a house)",
    "  Pink railroads   $5 fare; $10, $20, $50 with 2, 3, 4 (MONOPOLY)",
    "  Yellow           Soakum Lighting and Slambang Trolley: $5, or $25",
    "                   with both (the MUNICIPAL CINCH)",
    "  Blue             Food, Shelter, Fuel, Clothing: $10 tax",
    "  Trespass         Lord Blueblood's Estate, D.F. Hogg's Game",
    "                   Preserves: go to Jail",
    "  Luxury           $75 for a card worth 100, or move backward",
    "  Speculation      the broker's game: $10 ante, throw again",
    "  Chance           draw a red card",
    "  Legacy           $100",
    "  Mother Earth     $100 wages each time you reach or pass it",
    "  Central Park     free",
    "",
    "THE COMPUTER PLAYERS",
    "  Easy plays on impulse, Medium by rules of thumb, Hard counts the",
    "  odds, keeps cash for the rents ahead and hunts monopolies.",
    "",
    "Click any space on the board to read about it.",
    NULL
};

const char *const help_rules[] = {
    "THE LANDLORD'S GAME - the rules of 1906 (Economic Game Co., New York)",
    "as played by this program. Rule numbers are those of the 1906 rules.",
    "",
    "1  The cards are dealt round; each player may buy those dealt to him",
    "   at the sale price, paid into the PUBLIC TREASURY. Two players start",
    "   with $600 each, three with $500, four with $400.",
    "2  Throw the dice and move. Between the Chances (the last side) you",
    "   may move in either direction.",
    "3  Wages: $100 on reaching or passing MOTHER EARTH.",
    "4  Taxes: $10 on the Absolute Necessities.",
    "5  Lord Blueblood's Estate and D. F. Hogg's Game Preserves are land",
    "   held out of use: trespassers go to JAIL.",
    "6  Jail: pay $50, or throw a double to come out free. Not more than",
    "   three turns in Jail if you can raise the fine.",
    "7  Land rent is paid to the owner. An unowned lot is for sale: other",
    "   players may bid more; you have the first option at the highest",
    "   bid. If nobody buys it, its land rent goes to the Treasury.",
    "8  Speculation: you may refuse to move onto it. Ante $10 to the",
    "   broker, throw again: double $100, 11 $90, 10 $80 ... 5 $30, the",
    "   broker taking 10%. A 3 or 4 is a skin game: no winnings, and the",
    "   broker goes to Jail and loses his license.",
    "9  Franchises: $5. MUNICIPAL CINCH (both) $25. MONOPOLY: 2 railroads",
    "   $10, 3 $20, 4 $50. TRUST: two players owning all the railroads",
    "   may pool them; fares $40, profits divided.",
    "10 Central Park is maintained by public funds: free.",
    "11 Chance: draw a red card and follow it.",
    "12 Poor House: if you cannot pay and cannot raise the money, you go",
    "   there until your next turn.",
    "13 Luxury: pay $75 for a luxury card (counts 100), or move backward",
    "   from your last space as far as you threw.",
    "14 Houses: $100 each, on your own lots; $10 rent each, added to the",
    "   land rent.",
    "17 A double is an official pass: jump the nine spaces between the",
    "   next two corners, if your throw takes you past the first corner.",
    "19 The game ends when one player has received his wages five times.",
    "",
    "FOR ADVANCED AND SCIENTIFIC PLAYERS (Options > Game Settings)",
    "   Taxes double at 10 houses on the board ($20) and again at 25",
    "   ($40). The railroads and corners divide the board into eight",
    "   sections; when every lot in a section has a house the land rent",
    "   there doubles, at two houses each doubles again, at three again.",
    "   Not more than three houses on a lot.",
    "",
    "THE SINGLE TAX (Game > Propose the Single Tax)",
    "   Comes into force when two players vote for it. Then:",
    "   - no taxes on necessities;",
    "   - all land rent goes into the Public Treasury (which starts empty);",
    "     title deeds to lots are worth nothing;",
    "   - anyone may build a house on an unimproved lot and collect the",
    "     $10 house rent;",
    "   - each $50 in the Treasury buys a utility for the public (Soakum,",
    "     Slambang, then the railroads), which becomes free; then builds a",
    "     FREE COLLEGE on Lord Blueblood's Estate and on Hogg's Preserves",
    "     (four Education cards make a Professor, worth 100); then raises",
    "     wages by $10;",
    "   - there is no Poor House: a player who cannot pay rests on the",
    "     Natural Opportunity (corner) behind him and draws $50 from it.",
    "",
    "WHERE THIS PROGRAM DECIDES (Rule 18 leaves it to the players):",
    "   cards may be sold to other players or back to the pack at half",
    "   price, houses back to the bank for $50; no borrowing.",
    NULL
};

const char *const help_history[] = {
    "LIZZIE MAGIE AND THE LANDLORD'S GAME",
    "",
    "Elizabeth J. \"Lizzie\" Magie (1866-1948) of Macomb, Illinois, was a",
    "writer, stenographer, actress and inventor, and a follower of the",
    "economist Henry George, who argued for a single tax on the value of",
    "land in place of all other taxes.",
    "",
    "To show what George taught, she designed The Landlord's Game, tried",
    "it out in the single-tax colony of Arden, Delaware, and patented it:",
    "US Patent 748,626, filed March 23, 1903 and granted January 5, 1904.",
    "Its board was a continuous circuit - a novelty then - with lots for",
    "sale or rent, railroads, franchises, taxes on necessities, Jail and",
    "the Poor House, and the corner LABOR UPON MOTHER EARTH PRODUCES WAGES.",
    "",
    "The Economic Game Company of New York published it in 1906 with two",
    "sets of rules: the ordinary rules, under which one player ends by",
    "owning everything (\"the land monopolist is monarch of the world\"),",
    "and the Single Tax rules, under which the rent of land is shared and",
    "wages rise for all. She hoped children who played it would see the",
    "unfairness for themselves.",
    "",
    "The game spread by hand among students and Quakers, who drew their",
    "own boards. One such board reached Charles Darrow, whose version",
    "Parker Brothers sold as Monopoly from 1935; the firm bought Magie's",
    "1924 patent for $500. Her part in the story was largely forgotten",
    "until the 1970s.",
    "",
    "SOURCES",
    "  US Patent 748,626 (1904) and 1,509,312 (1924), public domain.",
    "  The 1906 rules of the Economic Game Co., as transcribed at",
    "  landlordsgame.info (Thomas Forsyth).",
    "  Wikipedia, \"The Landlord's Game\" and \"Elizabeth Magie\".",
    "  The Public Domain Review, \"The Landlord's Game\".",
    "  Mary Pilon, The Monopolists (2015).",
    NULL
};

const char *const help_keys[] = {
    "Space, Enter, R   roll the dice (on your turn)",
    "B                 build a house",
    "T                 buy a card from a computer player",
    "S                 sell a card to a computer player",
    "X                 propose the Single Tax",
    "F2                the standings",
    "PgUp / PgDn       scroll the record of events; End: the newest",
    "Ctrl+N            new game",
    "Ctrl+Q            quit",
    "Esc, F9, F10      the menu bar; Alt+G / Alt+O / Alt+H open a menu",
    "F1                how to play",
    "",
    "In dialogs: Enter presses the button with the black focus box,",
    "Tab or the arrow keys move it, Esc backs out, an underlined letter",
    "presses its button. In lists, Space ticks a box.",
    "While the computer players move, any key hurries them along.",
    "",
    "Click a space on the board to read about it.",
    NULL
};

const char *const help_license[] = {
    "Viola the Landlord",
    "Copyright (C) 2026 Lee Curtis",
    "",
    "This program is free software: you can redistribute it and/or modify",
    "it under the terms of the GNU General Public License as published by",
    "the Free Software Foundation, either version 3 of the License, or",
    "(at your option) any later version.",
    "",
    "This program is distributed in the hope that it will be useful,",
    "but WITHOUT ANY WARRANTY; without even the implied warranty of",
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
    "GNU General Public License for more details.",
    "",
    "You should have received a copy of the GNU General Public License",
    "along with this program.  If not, see <https://www.gnu.org/licenses/>.",
    "",
    "THE ARTWORK",
    "The board follows the layout of Lizzie J. Magie's patent drawing",
    "(US 748,626, 1904), which is in the public domain. The rules come",
    "from the patent and the 1906 rules, both in the public domain. Every",
    "picture in the program is original pixel art made for it, released",
    "under the GPL and also dedicated to the public domain (CC0 1.0).",
    "The fonts are the X server's own.",
    NULL
};
