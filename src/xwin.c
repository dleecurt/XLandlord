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

/* xwin.c - see xwin.h.
 *
 * Xlib primer, for the parts used here:
 *   Display   the connection to the X server
 *   Window    our top-level window
 *   Pixmap    an off-screen image: everything is drawn into `back` and copied
 *             to the window in one go (xw_flush), which avoids flicker
 *   GC        "graphics context": the current color, font, clip area
 * Every call goes into Xlib's output buffer; XFlush sends it.
 *
 * All the game's text is plain ASCII, so plain core fonts (XFontStruct) do;
 * no font sets or input methods are needed. */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include "xwin.h"
#include "art.h"
#include "keys.h"
#include "util.h"

XwMouse xw_mouse;

static Display *dpy;            /* the connection to the X server */
static int scr;                 /* its default screen */
static Window win;              /* our one top-level window */
static Pixmap back;             /* the off-screen copy everything is drawn into */
static GC gc;                   /* color, font and clipping for all drawing */
static int W, H;                /* the window's size */
static int depth;               /* bits per pixel of the screen */
static Atom wm_delete;          /* WM_DELETE_WINDOW: the close button */
static Pixmap icon_pixmap, icon_mask;   /* the classic X icon, for older window managers */

/* ---- colors ---- */

static const char *const color_spec[XC_COUNT] = {
    [XC_SURFACE]   = "#f0f0f0",     /* the Viola design system tokens */
    [XC_INK]       = "#000000",
    [XC_LIGHT]     = "#f9f9f9",
    [XC_DARK]      = "#c0c0c0",
    [XC_TROUGH]    = "#909090",
    [XC_MARK]      = "#fffacd",
    [XC_LOT]       = "#d4e6c8",
    [XC_NECESSITY] = "#cbd9ea",
    [XC_FRANCHISE] = "#f2e7a8",
    [XC_RAILROAD]  = "#efd0d4",
    [XC_LUXURY]    = "#dccfe6",
    [XC_CHANCE]    = "#ecc4b8",
    [XC_TRESPASS]  = "#d9d2c4",
    [XC_CORNER]    = "#f6f3ea",
    [XC_PLAYER0]   = "#c00000",     /* red: the human */
    [XC_PLAYER1]   = "#0050b5",     /* blue */
    [XC_PLAYER2]   = "#1e7b1e",     /* green */
    [XC_PLAYER3]   = "#806a00",     /* brass */
    [XC_WHITE]     = "#ffffff",
};
static unsigned long pixels[XC_COUNT];      /* XC_* -> the server's pixel value */

/* Allocate the palette (falling back to black or white on a full 8-bit
 * display). */
static void alloc_colors(void)
{
    Colormap cm = DefaultColormap(dpy, scr);
    for (int i = 0; i < XC_COUNT; i++) {
        XColor c;
        if (XParseColor(dpy, cm, color_spec[i], &c) && XAllocColor(dpy, cm, &c))
            pixels[i] = c.pixel;
        else            /* a full 8-bit display: black or white is better than nothing */
            pixels[i] = i == XC_INK || i >= XC_PLAYER0 ? BlackPixel(dpy, scr) : WhitePixel(dpy, scr);
    }
}

/* ---- fonts ---- */

static XFontStruct *fonts[XF_COUNT];        /* XF_* -> the font loaded for it */

/* Viola's fonts first (Adobe Helvetica, misc-fixed), then looser patterns
 * so a scalable font can stand in. Each list names the size in pixels. */
static const char *const ui_names[] = {
    "-adobe-helvetica-medium-r-normal--17-*-*-*-p-*-iso8859-1",
    "-*-helvetica-medium-r-normal--17-*-*-*-p-*-*-*",
    "-*-*-medium-r-normal--17-*-*-*-p-*-*-*",
    "-*-*-*-r-*--17-*-*-*-*-*-*-*",
    "variable", "fixed", NULL
};
static const char *const small_names[] = {
    "-adobe-helvetica-medium-r-normal--11-*-*-*-p-*-iso8859-1",
    "-*-helvetica-medium-r-normal--11-*-*-*-p-*-*-*",
    "-*-*-medium-r-normal--11-*-*-*-p-*-*-*",
    "-*-*-*-r-*--11-*-*-*-*-*-*-*",
    "6x10", "fixed", NULL
};
static const char *const bold_names[] = {
    "-adobe-helvetica-bold-r-normal--12-*-*-*-p-*-iso8859-1",
    "-*-helvetica-bold-r-normal--12-*-*-*-p-*-*-*",
    "-*-*-bold-r-normal--12-*-*-*-p-*-*-*",
    "-*-*-bold-r-*--12-*-*-*-*-*-*-*",
    "6x13bold", "fixed", NULL
};
static const char *const fixed_names[] = {
    "-misc-fixed-medium-r-semicondensed--13-*-*-*-c-60-iso8859-1",
    "6x13",
    "-*-*-medium-r-normal--13-*-*-*-c-*-*-*",
    "-*-*-medium-r-normal--13-*-*-*-m-*-*-*",
    "fixed", NULL
};
static const char *const logo_names[] = {
    "-adobe-times-bold-r-normal--24-*-*-*-p-*-iso8859-1",
    "-*-times-bold-r-normal--24-*-*-*-p-*-*-*",
    "-*-*-bold-r-normal--24-*-*-*-p-*-*-*",
    "-*-*-bold-r-*--24-*-*-*-*-*-*-*",
    NULL
};

/* The first of `names` the server has, or NULL. */
static XFontStruct *load_font(const char *const *names)
{
    for (int i = 0; names[i]; i++) {
        XFontStruct *f = XLoadQueryFont(dpy, names[i]);
        if (f) return f;
    }
    return NULL;
}

/* See xwin.h. */
int xw_ascent(int font)  { return fonts[font]->ascent; }
int xw_descent(int font) { return fonts[font]->descent; }

/* See xwin.h. */
int xw_text(int font, int x, int y, const char *s, int n, int color)
{
    if (n <= 0) return 0;
    XSetForeground(dpy, gc, pixels[color]);
    XSetFont(dpy, gc, fonts[font]->fid);
    XDrawString(dpy, back, gc, x, y, s, n);
    return XTextWidth(fonts[font], s, n);
}

/* See xwin.h: the text drawn once at its own size into a one-bit pixmap,
 * read back, and each set pixel drawn as a scale x scale square - a large
 * title from the X server's ordinary fonts, with the blocky look of the
 * game's pixel art. */
int xw_text_scaled(int font, int x, int y, const char *s, int n, int scale, int color)
{
    if (n <= 0 || scale < 1) return 0;
    XFontStruct *f = fonts[font];
    int w = XTextWidth(f, s, n), h = f->ascent + f->descent;
    if (w <= 0 || h <= 0) return 0;
    Pixmap pm = XCreatePixmap(dpy, win, (unsigned)w, (unsigned)h, 1);
    GC g1 = XCreateGC(dpy, pm, 0, NULL);
    XSetForeground(dpy, g1, 0);
    XFillRectangle(dpy, pm, g1, 0, 0, (unsigned)w, (unsigned)h);
    XSetForeground(dpy, g1, 1);
    XSetFont(dpy, g1, f->fid);
    XDrawString(dpy, pm, g1, 0, f->ascent, s, n);
    XImage *im = XGetImage(dpy, pm, 0, 0, (unsigned)w, (unsigned)h, 1, XYPixmap);
    XSetForeground(dpy, gc, pixels[color]);
    if (im) {
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; ) {
                if (!XGetPixel(im, i, j)) { i++; continue; }
                int k = i;
                while (k < w && XGetPixel(im, k, j)) k++;        /* a run of set pixels */
                XFillRectangle(dpy, back, gc, x + i * scale, y - f->ascent * scale + j * scale,
                               (unsigned)((k - i) * scale), (unsigned)scale);
                i = k;
            }
        }
        XDestroyImage(im);
    }
    XFreeGC(dpy, g1);
    XFreePixmap(dpy, pm);
    return w * scale;
}

/* See xwin.h. */
int xw_text_width(int font, const char *s, int n)
{
    return n > 0 ? XTextWidth(fonts[font], s, n) : 0;
}

/* ---- drawing ---- */

void xw_fill(int x, int y, int w, int h, int color)
{
    if (w <= 0 || h <= 0) return;
    XSetForeground(dpy, gc, pixels[color]);
    XFillRectangle(dpy, back, gc, x, y, (unsigned)w, (unsigned)h);
}

/* See xwin.h: one-pixel lines. */
void xw_hline(int x, int y, int w, int color) { xw_fill(x, y, w, 1, color); }
void xw_vline(int x, int y, int h, int color) { xw_fill(x, y, 1, h, color); }

/* See xwin.h. */
void xw_line(int x1, int y1, int x2, int y2, int color)
{
    XSetForeground(dpy, gc, pixels[color]);
    XDrawLine(dpy, back, gc, x1, y1, x2, y2);
}

/* Two-tone 2px border, the corners split diagonally like Viola's GLDrawBorder. */
static void bevel(int x, int y, int w, int h, int tl, int br)
{
    if (w < 4 || h < 4) return;
    for (int i = 0; i < 2; i++) {
        xw_hline(x + i, y + i, w - 2 * i - 1, tl);
        xw_vline(x + i, y + i, h - 2 * i - 1, tl);
        xw_hline(x + i + 1, y + h - 1 - i, w - 2 * i - 1, br);
        xw_vline(x + w - 1 - i, y + i + 1, h - 2 * i - 1, br);
    }
}

/* See xwin.h: Viola's two bevels, light over dark or dark over light. */
void xw_raised(int x, int y, int w, int h) { bevel(x, y, w, h, XC_LIGHT, XC_DARK); }
void xw_sunken(int x, int y, int w, int h) { bevel(x, y, w, h, XC_DARK, XC_LIGHT); }

/* See xwin.h. */
void xw_box(int x, int y, int w, int h, int color)
{
    if (w < 2 || h < 2) return;
    XSetForeground(dpy, gc, pixels[color]);
    XDrawRectangle(dpy, back, gc, x, y, (unsigned)(w - 1), (unsigned)(h - 1));
}

/* See xwin.h. */
void xw_triangle(int x, int y, int size, int dir, int color)
{
    int half = size / 2, q = size / 4;
    int down[6] = { x, y + q, x + size, y + q, x + half, y + q + half };
    int up[6]   = { x + half, y + q, x + size, y + q + half, x, y + q + half };
    xw_polygon(dir == 0 ? down : up, 3, color);
}

/* See xwin.h. */
void xw_disc(int x, int y, int d, int color)
{
    XSetForeground(dpy, gc, pixels[color]);
    XFillArc(dpy, back, gc, x, y, (unsigned)d, (unsigned)d, 0, 360 * 64);
}

/* See xwin.h. */
void xw_circle(int x, int y, int d, int color)
{
    XSetForeground(dpy, gc, pixels[color]);
    XDrawArc(dpy, back, gc, x, y, (unsigned)(d - 1), (unsigned)(d - 1), 0, 360 * 64);
}

/* See xwin.h. */
void xw_polygon(const int *xy, int npoints, int color)
{
    XPoint p[16];
    npoints = MIN(npoints, 16);
    for (int i = 0; i < npoints; i++) { p[i].x = (short)xy[2 * i]; p[i].y = (short)xy[2 * i + 1]; }
    XSetForeground(dpy, gc, pixels[color]);
    XFillPolygon(dpy, back, gc, p, npoints, Complex, CoordModeOrigin);
}

/* See xwin.h: batched into two XFillRectangles calls. */
void xw_picture(int x, int y, const char *const *rows, int scale, int color, int color2)
{
    XRectangle r1[1024], r2[1024];
    int n1 = 0, n2 = 0;
    for (int j = 0; rows[j]; j++) {
        for (int i = 0; rows[j][i]; i++) {
            char c = rows[j][i];
            XRectangle r = { (short)(x + i * scale), (short)(y + j * scale),
                             (unsigned short)scale, (unsigned short)scale };
            if (c == '#' && n1 < 1024) r1[n1++] = r;
            else if (c == '+' && color2 >= 0 && n2 < 1024) r2[n2++] = r;
        }
    }
    XSetForeground(dpy, gc, pixels[color]);
    if (n1) XFillRectangles(dpy, back, gc, r1, n1);
    if (n2) {
        XSetForeground(dpy, gc, pixels[color2]);
        XFillRectangles(dpy, back, gc, r2, n2);
    }
}

/* ---- full-color pictures ---- */

#define MAX_IMAGES 32
static Pixmap images[MAX_IMAGES];           /* decoded pictures, by handle */
static int image_w[MAX_IMAGES], image_h[MAX_IMAGES], nimages;

/* The server's pixel for an 0xRRGGBB color, remembered between calls
 * (black if the colormap is full). */
static unsigned long rgb_pixel(unsigned long rgb)
{
    static unsigned long seen_rgb[128], seen_pixel[128];
    static int nseen;
    for (int i = 0; i < nseen; i++) if (seen_rgb[i] == rgb) return seen_pixel[i];
    XColor c;
    c.red = (unsigned short)(((rgb >> 16) & 0xff) * 257);
    c.green = (unsigned short)(((rgb >> 8) & 0xff) * 257);
    c.blue = (unsigned short)((rgb & 0xff) * 257);
    c.flags = DoRed | DoGreen | DoBlue;
    unsigned long pixel = XAllocColor(dpy, DefaultColormap(dpy, scr), &c) ? c.pixel : BlackPixel(dpy, scr);
    if (nseen < 128) { seen_rgb[nseen] = rgb; seen_pixel[nseen] = pixel; nseen++; }
    return pixel;
}

/* See xwin.h: runs are drawn as rectangles, split where they wrap a row. */
int xw_image_new(int w, int h, const unsigned long *palette, int ncolors,
                 const unsigned char *rle, size_t size)
{
    if (nimages == MAX_IMAGES || w <= 0 || h <= 0 || ncolors <= 0 || ncolors > 256) return -1;
    unsigned long pix[256];
    for (int i = 0; i < ncolors; i++) pix[i] = rgb_pixel(palette[i]);
    Pixmap pm = XCreatePixmap(dpy, win, (unsigned)w, (unsigned)h, (unsigned)depth);
    GC g = XCreateGC(dpy, pm, 0, NULL);
    XSetForeground(dpy, g, pixels[XC_SURFACE]);
    XFillRectangle(dpy, pm, g, 0, 0, (unsigned)w, (unsigned)h);
    long pos = 0, total = (long)w * h;
    int last = -1;
    for (size_t i = 0; i < size && pos < total; ) {
        int color = rle[i++];
        unsigned long len = 0;
        int shift = 0;
        while (i < size) {
            unsigned b = rle[i++];
            len |= (unsigned long)(b & 0x7f) << shift;
            shift += 7;
            if (!(b & 0x80)) break;
        }
        long run = (long)len + 1;
        if (color >= ncolors) color = 0;
        if (color != last) { XSetForeground(dpy, g, pix[color]); last = color; }
        while (run > 0 && pos < total) {
            int x = (int)(pos % w), y = (int)(pos / w);
            int n = (int)MIN(run, (long)(w - x));
            XFillRectangle(dpy, pm, g, x, y, (unsigned)n, 1);
            pos += n;
            run -= n;
        }
    }
    XFreeGC(dpy, g);
    images[nimages] = pm;
    image_w[nimages] = w;
    image_h[nimages] = h;
    return nimages++;
}

/* See xwin.h. */
void xw_image_draw(int id, int sx, int sy, int w, int h, int x, int y)
{
    if (id < 0 || id >= nimages) return;
    w = MIN(w, image_w[id] - sx);
    h = MIN(h, image_h[id] - sy);
    if (w <= 0 || h <= 0) return;
    XCopyArea(dpy, images[id], back, gc, sx, sy, (unsigned)w, (unsigned)h, x, y);
}

/* See xwin.h. */
void xw_clip(int x, int y, int w, int h)
{
    XRectangle r = { (short)x, (short)y, (unsigned short)MAX(0, w), (unsigned short)MAX(0, h) };
    XSetClipRectangles(dpy, gc, 0, 0, &r, 1, Unsorted);
}

/* See xwin.h. */
void xw_clip_off(void) { XSetClipMask(dpy, gc, None); }

/* ---- the window ---- */

int xw_width(void)  { return W; }
int xw_height(void) { return H; }

/* (Re)make the off-screen copy at the window's size. */
static void make_back(void)
{
    if (back) XFreePixmap(dpy, back);
    back = XCreatePixmap(dpy, win, (unsigned)MAX(1, W), (unsigned)MAX(1, H), (unsigned)depth);
    xw_fill(0, 0, W, H, XC_SURFACE);
}

/* See xwin.h. */
void xw_flush(void)
{
    XCopyArea(dpy, back, win, gc, 0, 0, (unsigned)W, (unsigned)H, 0, 0);
    XFlush(dpy);
}

/* See xwin.h. */
void xw_bell(void) { XBell(dpy, 0); XFlush(dpy); }

/* See xwin.h. */
void xw_set_title(const char *title)
{
    XStoreName(dpy, win, title);
    XSetIconName(dpy, win, title);
}

/* See xwin.h. */
int xw_open(void)
{
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        const char *d = getenv("DISPLAY");
        fprintf(stderr, "XLandlord: cannot open the X display %s\n"
                        "(XLandlord is an X11 program: run it inside an X session, or set DISPLAY.)\n",
                d && *d ? d : "(DISPLAY is not set)");
        return 0;
    }
    scr = DefaultScreen(dpy);
    depth = DefaultDepth(dpy, scr);
    alloc_colors();
    fonts[XF_UI] = load_font(ui_names);
    fonts[XF_SMALL] = load_font(small_names);
    fonts[XF_BOLD] = load_font(bold_names);
    fonts[XF_FIXED] = load_font(fixed_names);
    fonts[XF_LOGO] = load_font(logo_names);
    if (!fonts[XF_UI] || !fonts[XF_FIXED]) {
        fprintf(stderr, "XLandlord: no usable X font found (install xfonts-base)\n");
        XCloseDisplay(dpy);
        return 0;
    }
    if (!fonts[XF_SMALL]) fonts[XF_SMALL] = fonts[XF_FIXED];
    if (!fonts[XF_BOLD]) fonts[XF_BOLD] = fonts[XF_SMALL];
    if (!fonts[XF_LOGO]) fonts[XF_LOGO] = fonts[XF_UI];
    return 1;
}

/* ---- the program icon ---- */

/* Modern window managers read _NET_WM_ICON (ARGB pixels); older ones (twm,
 * fvwm, mwm: the Viola era) want an icon Pixmap and a mask in WM_HINTS. Both
 * are made from the 32x32 picture in art.c, drawn at 1x and 2x. */
static void set_icons(void)
{
    int s = ICON_SIZE;
    size_t n = (size_t)(2 + s * s) + (size_t)(2 + 4 * s * s);
    unsigned long *data = xrealloc(NULL, n * sizeof *data);
    size_t at = 0;
    for (int scale = 1; scale <= 2; scale++) {
        int z = s * scale;
        data[at++] = (unsigned long)z;
        data[at++] = (unsigned long)z;
        for (int y = 0; y < z; y++)
            for (int x = 0; x < z; x++) data[at++] = art_icon_argb(x / scale, y / scale);
    }
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_ICON", False), XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)data, (int)at);
    free(data);

    Window root = RootWindow(dpy, scr);
    icon_pixmap = XCreatePixmap(dpy, root, (unsigned)s, (unsigned)s, (unsigned)depth);
    icon_mask = XCreatePixmap(dpy, root, (unsigned)s, (unsigned)s, 1);
    GC g = XCreateGC(dpy, icon_pixmap, 0, NULL), gm = XCreateGC(dpy, icon_mask, 0, NULL);
    XSetForeground(dpy, gm, 0);
    XFillRectangle(dpy, icon_mask, gm, 0, 0, (unsigned)s, (unsigned)s);
    XSetForeground(dpy, gm, 1);
    Colormap cm = DefaultColormap(dpy, scr);
    unsigned long seen_argb[16], seen_pixel[16];
    int nseen = 0;
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            unsigned long argb = art_icon_argb(x, y);
            if ((argb >> 24) < 0x80) continue;
            int k;
            for (k = 0; k < nseen && seen_argb[k] != argb; k++) ;
            unsigned long pixel = BlackPixel(dpy, scr);
            if (k < nseen) pixel = seen_pixel[k];
            else {
                XColor c;
                c.red = (unsigned short)(((argb >> 16) & 0xff) * 257);
                c.green = (unsigned short)(((argb >> 8) & 0xff) * 257);
                c.blue = (unsigned short)((argb & 0xff) * 257);
                c.flags = DoRed | DoGreen | DoBlue;
                if (XAllocColor(dpy, cm, &c)) pixel = c.pixel;
                if (nseen < 16) { seen_argb[nseen] = argb; seen_pixel[nseen] = pixel; nseen++; }
            }
            XSetForeground(dpy, g, pixel);
            XDrawPoint(dpy, icon_pixmap, g, x, y);
            XDrawPoint(dpy, icon_mask, gm, x, y);
        }
    XFreeGC(dpy, g);
    XFreeGC(dpy, gm);
}

/* See xwin.h: size hints, class, icons, the close button, then wait for the
 * map. */
void xw_show(int argc, char **argv, int w, int h)
{
    W = w > 0 ? w : 1120;
    H = h > 0 ? h : 800;
    win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, (unsigned)W, (unsigned)H, 0,
                              pixels[XC_INK], pixels[XC_SURFACE]);
    XSizeHints *sh = XAllocSizeHints();
    sh->flags = PMinSize | PSize;
    sh->min_width = 800;
    sh->min_height = 560;
    sh->width = W;
    sh->height = H;
    XClassHint *ch = XAllocClassHint();
    ch->res_name = "XLandlord";
    ch->res_class = "XLandlord";
    set_icons();
    XWMHints *wh = XAllocWMHints();
    wh->flags = InputHint | StateHint | IconPixmapHint | IconMaskHint;
    wh->input = True;
    wh->initial_state = NormalState;
    wh->icon_pixmap = icon_pixmap;
    wh->icon_mask = icon_mask;
    XSetWMProperties(dpy, win, NULL, NULL, argv, argc, sh, wh, ch);
    XFree(sh); XFree(ch); XFree(wh);
    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wm_delete, 1);

    gc = XCreateGC(dpy, win, 0, NULL);
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    make_back();
    xw_set_title("XLandlord");
    XMapWindow(dpy, win);
    for (;;) {                  /* wait until it is on screen, so the first drawing is not lost */
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == MapNotify) break;
    }
}

/* See xwin.h. */
void xw_close(void)
{
    if (!dpy) return;
    for (int i = 0; i < XF_COUNT; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) if (fonts[j] == fonts[i]) dup = 1;
        if (fonts[i] && !dup) XFreeFont(dpy, fonts[i]);
    }
    if (back) XFreePixmap(dpy, back);
    for (int i = 0; i < nimages; i++) XFreePixmap(dpy, images[i]);
    nimages = 0;
    if (icon_pixmap) XFreePixmap(dpy, icon_pixmap);
    if (icon_mask) XFreePixmap(dpy, icon_mask);
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    dpy = NULL;
}

/* ---- input ---- */

static int translate_key(XKeyEvent *e)
{
    char text[16];
    KeySym ks = NoSymbol;
    int n = XLookupString(e, text, (int)sizeof text - 1, &ks, NULL);
    int shift = (e->state & ShiftMask) != 0;
    int alt = (e->state & Mod1Mask) != 0;
    switch (ks) {
    case XK_Up:    case XK_KP_Up:    return K_UP;
    case XK_Down:  case XK_KP_Down:  return K_DOWN;
    case XK_Left:  case XK_KP_Left:  return K_LEFT;
    case XK_Right: case XK_KP_Right: return K_RIGHT;
    case XK_Home:  case XK_KP_Home:  return K_HOME;
    case XK_End:   case XK_KP_End:   return K_END;
    case XK_Prior: case XK_KP_Prior: return K_PGUP;
    case XK_Next:  case XK_KP_Next:  return K_PGDN;
    case XK_BackSpace:               return K_BACKSPACE;
    case XK_Delete: case XK_KP_Delete: return K_DELETE;
    case XK_Return: case XK_KP_Enter: return '\n';
    case XK_Escape:                  return 27;
    case XK_Tab:                     return shift ? K_BTAB : '\t';
    case XK_ISO_Left_Tab:            return K_BTAB;
    default: break;
    }
    if (ks >= XK_F1 && ks <= XK_F24) return K_F((int)(ks - XK_F1) + 1);
    if (alt) {
        KeySym base = XLookupKeysym(e, 0);
        if (base >= XK_a && base <= XK_z) return K_ALT((int)('a' + (base - XK_a)));
        if (base >= XK_0 && base <= XK_9) return K_ALT((int)('0' + (base - XK_0)));
        return ERR;
    }
    if (n == 1 && (unsigned char)text[0] >= 32 && (unsigned char)text[0] < 127) return (unsigned char)text[0];
    if (n == 1 && (text[0] == 14 || text[0] == 17 || text[0] == 21))
        return text[0];                             /* Ctrl+N, Ctrl+Q, Ctrl+U */
    return ERR;
}

/* Fill xw_mouse from a press; two quick presses of one button in one place
 * make a double click. */
static void read_button(XButtonEvent *b)
{
    static Time last_time;
    static int last_button, last_x, last_y;
    xw_mouse.x = b->x;
    xw_mouse.y = b->y;
    xw_mouse.button = (int)b->button;
    int dbl = b->button == (unsigned)last_button && b->time - last_time < 400 &&
              abs(b->x - last_x) < 5 && abs(b->y - last_y) < 5;
    xw_mouse.clicks = dbl ? 2 : 1;
    last_time = dbl ? 0 : b->time;
    last_button = (int)b->button;
    last_x = b->x;
    last_y = b->y;
}

/* Handle one event; return a key code or ERR. */
static int handle(XEvent *ev)
{
    switch (ev->type) {
    case Expose:
        XCopyArea(dpy, back, win, gc, ev->xexpose.x, ev->xexpose.y,
                  (unsigned)ev->xexpose.width, (unsigned)ev->xexpose.height,
                  ev->xexpose.x, ev->xexpose.y);
        XFlush(dpy);
        return ERR;
    case ConfigureNotify:
        if (ev->xconfigure.width != W || ev->xconfigure.height != H) {
            W = ev->xconfigure.width;
            H = ev->xconfigure.height;
            make_back();
            return K_RESIZE;
        }
        return ERR;
    case ClientMessage:
        return (Atom)ev->xclient.data.l[0] == wm_delete ? K_CLOSE : ERR;
    case MappingNotify:
        XRefreshKeyboardMapping(&ev->xmapping);
        return ERR;
    case KeyPress:
        return translate_key(&ev->xkey);
    case ButtonPress:
        read_button(&ev->xbutton);
        return K_MOUSE;
    default:
        return ERR;
    }
}

static int (*idle_fn)(void);    /* xw_set_idle */
static int idle_every = 250, in_idle_fn;

/* See xwin.h. */
void xw_set_idle(int (*fn)(void), int ms) { idle_fn = fn; idle_every = MAX(20, ms); }

/* See xwin.h: the next key, click or window event, waiting as long as it
 * takes (calling the idle function meanwhile, if one is set). */
int xw_next_key(void)
{
    for (;;) {
        if (!idle_fn || in_idle_fn) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            int k = handle(&ev);
            if (k != ERR) return k;
            continue;
        }
        /* With an idle function: wait for X at most idle_every ms, then
         * call it; if it changed something, have the caller redraw. */
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            int k = handle(&ev);
            if (k != ERR) return k;
        }
        int fd = ConnectionNumber(dpy);
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        struct timeval tv = { idle_every / 1000, (idle_every % 1000) * 1000 };
        if (select(fd + 1, &set, NULL, NULL, &tv) > 0) continue;
        in_idle_fn = 1;
        int changed = idle_fn();
        in_idle_fn = 0;
        if (changed) return K_RESIZE;
    }
}

/* See xwin.h: select() on the X connection until the deadline. */
int xw_wait_key(int ms)
{
    struct timeval end, now;
    gettimeofday(&end, NULL);
    end.tv_sec += ms / 1000;
    end.tv_usec += (ms % 1000) * 1000;
    if (end.tv_usec >= 1000000) { end.tv_sec++; end.tv_usec -= 1000000; }
    int fd = ConnectionNumber(dpy);
    for (;;) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            int k = handle(&ev);
            if (k != ERR) return k;
        }
        gettimeofday(&now, NULL);
        long left = (end.tv_sec - now.tv_sec) * 1000000L + (end.tv_usec - now.tv_usec);
        if (left <= 0) return K_TIMEOUT;
        struct timeval tv = { left / 1000000L, left % 1000000L };
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        select(fd + 1, &set, NULL, NULL, &tv);
    }
}

/* See xwin.h: keys and clicks are dropped, other events handled. */
void xw_drain(void)
{
    while (XPending(dpy)) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress || ev.type == ButtonPress) continue;
        handle(&ev);
    }
}
