/*
 * XLandlord - The Landlord's Game for X11
 * Copyright (C) 2026 Lee Curtis
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

/* guide.c - Help > How to Play: an illustrated guide, one topic a page.
 *
 * Each page has a title, a screenshot (from guidepics.c) and a few
 * paragraphs. The picture stands to the left of the text when the dialog
 * is wide enough for both, and above it otherwise; text that does not fit
 * scrolls. Back / Next / Close at the bottom, the page number at the left.
 *
 * Keys: Left / Right (and Page Up / Page Down) turn the page, Home / End go
 * to the first / last page, Up / Down and the wheel scroll the text, Tab
 * moves the focus box, Enter or Space presses the focused button, B / N /
 * C press Back / Next / Close, Esc closes. */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "guide.h"
#include "guidepics.h"
#include "help.h"
#include "keys.h"
#include "ui.h"
#include "util.h"
#include "xwin.h"

typedef struct {
    const char *title;
    const char *pic;        /* a guide_pics name, or NULL */
    const char *text;       /* paragraphs, separated by blank lines */
} Page;

static const Page pages[] = {
    { "The Landlord's Game", "title",
      "Lizzie J. Magie patented The Landlord's Game in 1904 to show how land "
      "rent makes landlords rich and tenants poor. XLandlord plays it by the "
      "rules of 1906, including the Single Tax.\n\n"
      "The object: get as much wealth as you can. When one player has drawn "
      "his wages five times (you can choose 3, 7 or 10), the game ends and "
      "the richest player wins.\n\n"
      "Wealth is your cash plus 100 for every card you hold, every house you "
      "built and every luxury you bought - whatever you paid for them. A lot "
      "bought for $25 is worth $75 more than it cost before it collects a "
      "single rent.\n\n"
      "Use Next (or the Right arrow) to turn the page." },
    { "Starting", "start-up",
      "XLandlord opens on its title card, which asks how you want to play.\n\n"
      "Host a Network Game: you run the game and up to three players join "
      "from their own machines.\n\n"
      "Join a Network Game: you play in a game someone else is hosting.\n\n"
      "Play Here: everyone plays at this screen, against the computer or "
      "taking turns.\n\n"
      "Then it asks your name - the name you play under, shown to everyone "
      "and used in network games (not your login name).\n\n"
      "The same choices are in the Game menu at any time. From a terminal, "
      "XLandlord --host, --join HOST or --local makes the first choice." },
    { "The New Game form", "new-game",
      "Each of the four seats is Empty, a Human at this screen, or the "
      "Computer. You need two players at least, and one human; one seat at "
      "most may be the computer player.\n\n"
      "Computer skill - Easy, Medium or Hard - is how the computer player "
      "plays. Whoever sets up the game sets it for everyone.\n\n"
      "Game ends at wages: 5 is the 1906 rule.\n\n"
      "Rules: Standard, or Advanced - the 1906 rules for \"advanced and "
      "scientific players\", where taxes and land rents rise as houses go "
      "up.\n\n"
      "Each player starts on MOTHER EARTH with $600 if two play, $500 if "
      "three, $400 if four." },
    { "The deal", "the-deal",
      "The cards - lots, railroads, franchises and the broker's license - "
      "are dealt round. You may buy any of those dealt to you at the price "
      "on the card.\n\n"
      "Tick them with a click or Space (A ticks all, N none), then press "
      "OK. The line under the list shows the cost and what you would keep.\n\n"
      "Keep $50 to $75 in hand, as the 1906 rules advise: there are taxes "
      "and rents to pay on the way round. Cards nobody buys go back into "
      "the pack and are sold later to whoever stops on them." },
    { "Your turn", "your-turn",
      "Each player has a box: cash, worth, how many times he has drawn his "
      "wages, his cards and houses, and where he stands. The yellow box is "
      "the player whose turn it is.\n\n"
      "On your turn the line under the boxes says so and Roll Dice has the "
      "black focus box. Before you throw you may build houses (Build...), "
      "buy or sell cards (Trade...), or propose the Single Tax (Game "
      "menu).\n\n"
      "Then press Roll Dice, Space or Enter. Your checker moves forward by "
      "the throw. Each time you reach or pass MOTHER EARTH you draw $100 "
      "wages.\n\n"
      "Standings (F2) shows everyone's worth at any time." },
    { "Which way?", "which-way",
      "On the six spaces between the two Chances on the right side of the "
      "board (Rule 2) you may move backward instead of forward. The dialog "
      "says where each way would take you.\n\n"
      "Going backward can keep you off a dear lot, or bring you round to "
      "MOTHER EARTH and your wages sooner." },
    { "Doubles: the railroad pass", "railroad-pass",
      "A double is an official pass on the railroad (Rule 17). If your "
      "throw carries you past the next corner, you may jump the nine spaces "
      "to the corner after it and finish nine spaces further on.\n\n"
      "You may move straight ahead instead, if the pass would land you "
      "somewhere worse." },
    { "For sale", "for-sale",
      "When you stop on a lot, railroad or franchise nobody owns, it is for "
      "sale. The other players may bid more than the price; you have the "
      "first option, at the highest bid.\n\n"
      "If you pass and somebody bid, he buys it and you pay him the rent at "
      "once. If nobody bid, nobody buys it and you pay its land rent into "
      "the Public Treasury.\n\n"
      "On another player's card you pay its owner: the land rent of a lot "
      "plus $10 a house, or the railroad's fare." },
    { "Bidding", "bidding",
      "When another player stops on an unowned card, you are asked for the "
      "most you would pay. Enter 0 not to bid.\n\n"
      "The bidding goes up in $5 steps; the highest bidder pays $5 more than "
      "the runner-up would have, never more than his own limit. The player "
      "who stopped there can still take it at that price.\n\n"
      "Options > Ask Me to Bid turns these questions off." },
    { "The board", "board-corner",
      "Play runs from MOTHER EARTH, bottom right, along the bottom, up the "
      "left, across the top and down the right side. A railroad stands in "
      "the middle of each side.\n\n"
      "A small square in a space's corner shows its owner's color. Click any "
      "space to read about it: its price, rent and owner.\n\n"
      "Railroads pay their owner $5 a stop - $10 each with two, $20 with "
      "three, $50 with all four. Both franchises together (the Municipal "
      "Cinch) pay $25 each. Two players holding all four railroads may form "
      "a Trust: every fare becomes $40.\n\n"
      "Food, Shelter, Fuel and Clothing charge a $10 tax. Lord Blueblood's "
      "Estate and Hogg's Game Preserves are trespassing: go to Jail." },
    { "Chance, Luxury and Legacy", "chance",
      "Chance draws a card: money to pay or collect, or a move - to Jail, "
      "to MOTHER EARTH, to Central Park, to the next railroad, or back "
      "three spaces.\n\n"
      "Luxury: pay $75 for a luxury card, which counts 100 at the end - or "
      "move backward as far as you threw.\n\n"
      "Legacy pays you $100. Central Park is free." },
    { "Speculation", "speculation",
      "SPECULATION is the broker's game (Rule 8). You may refuse to move "
      "onto it. If you go, you pay a $10 ante to the broker - whoever holds "
      "the broker's license - and throw again.\n\n"
      "Any double wins $100; 11 wins $90, and so on down to $30 for a 5. A "
      "3 or 4 is a \"skin game\" and wins nothing - but then the broker is "
      "caught and goes to Jail.\n\n"
      "The broker takes 10% of every win." },
    { "Jail and the Poor House", "jail",
      "In Jail, each turn you either pay the $50 fine and throw as usual, "
      "or throw for a double: a double lets you out. On the third turn the "
      "fine is paid for you.\n\n"
      "When you owe more than you have, you must sell: a card to another "
      "player, a card back to the pack for half its price, or a house for "
      "$50. If there is nothing left to sell, you pay what you have and go "
      "to the Poor House until your next turn." },
    { "Building houses", "houses",
      "Before you throw you may build on your own lots: Build... or B. A "
      "house costs $100, adds $10 to the lot's rent, and counts 100 at the "
      "end.\n\n"
      "Houses show as small roofs on the board, in their owner's color. Up "
      "to nine may stand on a lot (three under the advanced rules, where "
      "the land rents of a fully built section double)." },
    { "Trading", "trade",
      "All cards may change hands before a throw. Trade... (or T) buys a "
      "card from another player at the price you name; S sells one of "
      "yours.\n\n"
      "A computer player takes an offer that beats what the card is worth "
      "to it. A human player is asked to accept or decline.\n\n"
      "Lots with houses on them cannot be traded until the houses are sold." },
    { "The Single Tax", "single-tax-vote",
      "In his turn any player may propose the Single Tax (Game menu, or X). "
      "It comes into force if a second player votes for it.\n\n"
      "Then title deeds to lots are worth nothing, the taxes on necessities "
      "are abolished, and all land rent goes into the Public Treasury. "
      "Owners still collect $10 a house." },
    { "When the Single Tax is in force", "single-tax",
      "The board says so, and its boxes show the Public Treasury and the "
      "wages.\n\n"
      "Every $50 in the Treasury is spent for the public: it buys the "
      "franchises and then the railroads from their owners, which then "
      "charge nothing; then it builds free colleges on Lord Blueblood's "
      "Estate and Hogg's Game Preserves; after that, it raises everyone's "
      "wages by $10.\n\n"
      "There is no Poor House: a player who cannot pay rests on the corner "
      "behind him." },
    { "The end of the game", "standings",
      "The game ends at the end of the turn in which a player draws his "
      "wages for the last time. The standings show each player's cash, "
      "cards, houses, luxuries and worth; the richest is the winner - \"the "
      "millionaire\".\n\n"
      "Ctrl+N starts a new game." },
    { "Playing over the network", "waiting-room",
      "One player hosts (Host a Network Game) and waits here; the others "
      "join from their own machines by typing the host's IP address or host "
      "name. The host presses Start Game when everyone is in.\n\n"
      "Host and Play: the host takes a seat and up to three others join. Host "
      "Only: the host watches without playing, and up to four join.\n\n"
      "The connection runs over ssh, so each joiner must be able to log in "
      "to the host with ssh without a password. Help > Network Play and the "
      "manual page (man 6 XLandlord) explain how to set that up.\n\n"
      "When fewer than four humans play, one computer player takes a free "
      "seat, at the skill the host set when he opened the game. Joiners "
      "have no say in it." },
    { "Who is where", "network-players",
      "In a network game each player's box shows the machine he plays from: "
      "the host's name, or the name of the joiner's machine - or its IP "
      "address when it has no name. \"you\" marks your own seat.\n\n"
      "Everyone sees the same board and dialogs; on your turn the buttons "
      "work.\n\n"
      "Once the game starts nobody new may join, and the computer player "
      "stays to the end. A player who quits is out: his cards go back into "
      "the pack and nobody takes his seat; his box shows he has left.\n\n"
      "If your connection drops, your game reconnects by itself and your "
      "seat waits for you for the time the host chose. If you take too long "
      "to answer, you are asked \"Are you still there?\" - answer within a "
      "minute or you are taken out so that play can go on. If the host "
      "leaves, the game is over.\n\n"
      "That is the whole game. Help > The Rules of 1906 has the rules in "
      "full, and Help > Keyboard every key.\n\n"
      "To print the rules: the rule book, with screenshots, a board "
      "map and a one-page quick reference, is installed as " RULEBOOK_PDF
      ". Open it with any PDF viewer. The manual page (man 6 XLandlord) "
      "has the rules too, with installation and setup." },
};
#define NPAGES ((int)(sizeof pages / sizeof pages[0]))

/* The picture called `name` as an image handle, decoding it the first
 * time; -1 if there is none. Fills in its size. */
static int picture(const char *name, int *w, int *h)
{
    static int handle[64];
    static char tried[64];
    *w = *h = 0;
    if (!name) return -1;
    char cname[64];
    copy_str(cname, sizeof cname, name);
    for (char *p = cname; *p; p++) if (*p == '-') *p = '_';
    for (int i = 0; guide_pics[i].name && i < 64; i++) {
        const GuidePic *gp = &guide_pics[i];
        if (strcmp(gp->name, cname)) continue;
        if (!tried[i]) {
            handle[i] = xw_image_new(gp->w, gp->h, gp->palette, gp->ncolors, gp->rle, gp->size);
            tried[i] = 1;
        }
        *w = gp->w;
        *h = gp->h;
        return handle[i];
    }
    return -1;
}

/* A sunken frame around a picture of w x h at (x, y). */
static void framed(int id, int x, int y, int w, int h, int sx, int sy)
{
    xw_box(x - 2, y - 2, w + 4, h + 4, XC_DARK);
    xw_box(x - 1, y - 1, w + 2, h + 2, XC_TROUGH);
    xw_image_draw(id, sx, sy, w, h, x, y);
}

/* The text column's width for a page with a pw-wide picture in a dialog
 * whose inside is inner_w wide; sets *side when picture and text stand
 * side by side (the text keeps 300 pixels at least). */
static int text_width(int pw, int inner_w, int *side)
{
    *side = pw > 0 && inner_w - pw - 24 >= 300;
    return *side ? inner_w - pw - 24 : inner_w;
}

/* The height page `pg` needs inside a dialog inner_w wide. */
static int needed_height(const Page *pg, int inner_w)
{
    int pw, ph, side;
    if (picture(pg->pic, &pw, &ph) < 0) pw = ph = 0;
    int tw = text_width(pw, inner_w, &side);
    char buf[4096];
    const char *lines[120];
    int nl = ui_wrap(XF_UI, pg->text, tw - 18, buf, sizeof buf, lines, 120);
    int text_h = nl * xw_line_height(XF_UI);
    return side ? MAX(ph + 4, text_h) : (ph ? ph + 16 : 0) + text_h;
}

/* See guide.h. */
void guide_show(void)
{
    static int page;            /* remembered between visits */
    int top = 0;                /* first text line shown */
    int focus = 1;              /* 0 Back, 1 Next, 2 Close */
    static const char *const labels[3] = { "< &Back", "&Next >", "&Close" };
    for (;;) {
        page = CLAMP(page, 0, NPAGES - 1);
        const Page *pg = &pages[page];
        int pw, ph;
        int pic = picture(pg->pic, &pw, &ph);
        if (pic < 0) pw = ph = 0;

        /* The dialog: up to 1080 wide, as tall as the tallest page needs
         * (so it keeps its size from page to page), within the window. */
        int lh = xw_line_height(XF_UI);
        int W = MIN(1080, xw_width() - 24);
        int chrome_h = 2 * UI_MARGIN + ui_title_height() + 16 + UI_BUTTON_H;
        int inner_w = W - 2 * UI_MARGIN, tallest = 0;
        for (int i = 0; i < NPAGES; i++) tallest = MAX(tallest, needed_height(&pages[i], inner_w));
        int H = MIN(chrome_h + tallest, xw_height() - 24);
        int inner_h = H - chrome_h;
        int side;
        text_width(pw, inner_w, &side);
        int tx, ty, tw, th, px, py, show_w, show_h;
        char title[160];
        snprintf(title, sizeof title, "How to Play - %s", pg->title);
        UiRect c = ui_frame(title, W, H);
        if (side) {
            show_w = pw;
            show_h = MIN(ph, inner_h - 4);
            px = c.x + 2;
            py = c.y + 2;
            tx = px + pw + 24;
            ty = c.y;
            tw = c.x + c.w - tx;
            th = inner_h;
        } else {
            show_w = MIN(pw, inner_w - 4);
            /* The picture first; the text keeps five lines and scrolls. */
            show_h = pw > 0 ? MAX(0, MIN(ph, inner_h - 5 * lh - 16)) : 0;
            px = c.x + (c.w - show_w) / 2;
            py = c.y + 2;
            tx = c.x;
            ty = c.y + (show_h ? show_h + 16 : 0);
            tw = c.w;
            th = inner_h - (ty - c.y);
        }
        char buf[4096];
        const char *lines[120];
        int scroll_w = 18;
        int nl = ui_wrap(XF_UI, pg->text, tw - scroll_w, buf, sizeof buf, lines, 120);
        int rows = MAX(1, th / lh);
        top = CLAMP(top, 0, MAX(0, nl - rows));
        if (pic >= 0) framed(pic, px, py, show_w, show_h, (pw - show_w) / 2, 0);
        xw_clip(tx, ty, tw - scroll_w, rows * lh);
        for (int i = 0; i < rows && top + i < nl; i++)
            xw_text(XF_UI, tx, ty + i * lh + xw_ascent(XF_UI), lines[top + i], (int)strlen(lines[top + i]), XC_INK);
        xw_clip_off();
        ui_scrollbar(tx + tw - 12, ty, rows * lh, top, rows, nl);

        /* The page number and the buttons. */
        char num[40];
        snprintf(num, sizeof num, "Page %d of %d", page + 1, NPAGES);
        int by = c.y + c.h - UI_BUTTON_H;
        xw_text(XF_UI, c.x, by + (UI_BUTTON_H - lh) / 2 + xw_ascent(XF_UI), num, (int)strlen(num), XC_INK);
        UiRect b[3];
        for (int i = 0; i < 3; i++) {
            b[i] = (UiRect){ c.x + c.w - (3 - i) * (UI_BUTTON_W + 10) + 10, by, UI_BUTTON_W, UI_BUTTON_H };
            ui_button(b[i], labels[i], i == focus);
        }
        xw_flush();

        /* Keys and clicks. */
        int k = xw_next_key(), press = -1;
        if (k == K_RESIZE) continue;
        if (k == K_MOUSE) {
            if (xw_mouse.button == 4) { top -= 3; continue; }
            if (xw_mouse.button == 5) { top += 3; continue; }
            if (xw_mouse.button != 1) continue;
            for (int i = 0; i < 3; i++) if (ui_hit(b[i], xw_mouse.x, xw_mouse.y)) press = i;
            if (press < 0) continue;
        }
        else if (k == 27 || k == K_CLOSE) return;
        else if (k == '\n' || k == '\r' || k == ' ') press = focus;
        else if (k == K_RIGHT || k == K_PGDN) press = 1;
        else if (k == K_LEFT || k == K_PGUP || k == K_BACKSPACE) press = 0;
        else if (k == K_HOME) { page = 0; top = 0; focus = 1; continue; }
        else if (k == K_END)  { page = NPAGES - 1; top = 0; focus = 2; continue; }
        else if (k == K_UP)   { top--; continue; }
        else if (k == K_DOWN) { top++; continue; }
        else if (k == '\t')   { focus = (focus + 1) % 3; continue; }
        else if (k == K_BTAB) { focus = (focus + 2) % 3; continue; }
        else if (k > 0 && k < 128) {
            int c2 = tolower(k);
            press = c2 == 'b' ? 0 : c2 == 'n' ? 1 : c2 == 'c' ? 2 : -1;
            if (press < 0) continue;
        }
        else continue;

        if (press == 2) return;
        int old = page;
        page += press == 0 ? -1 : 1;
        page = CLAMP(page, 0, NPAGES - 1);
        if (page != old) top = 0;
        /* At the last page the natural next step is Close. */
        focus = page == NPAGES - 1 ? 2 : page == 0 ? 1 : focus == 2 ? 1 : focus;
    }
}
