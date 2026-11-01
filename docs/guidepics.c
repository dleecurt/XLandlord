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

/* guidepics.c - make src/guidepics.c, the screenshots of Help > How to
 * Play, from binary PPM files (P6).
 *
 *   guidepics NAME=FILE.ppm ... > src/guidepics.c
 *
 * `make guide-pics' runs it, after ImageMagick has turned each
 * docs/guide/NAME.png into a PPM. Each picture is stored as a palette
 * (0xRRGGBB, at most 256 colors; the game's screenshots use a dozen or so)
 * and a run-length list over the whole picture, row after row:
 *
 *   color index (one byte), then the run length less one in 7-bit groups,
 *   lowest first, the top bit set on every group but the last.
 *
 * The generated file is kept in the source tree, so building the game
 * needs neither ImageMagick nor this program. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read a P6 PPM (maxval 255) into a malloc'd RGB buffer; 0 on failure. */
static unsigned char *read_ppm(const char *path, int *w, int *h)
{
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    int maxval = 0;
    char magic[3] = "";
    if (fscanf(f, "%2s", magic) != 1 || strcmp(magic, "P6")) goto bad;
    /* width, height, maxval, skipping comments */
    int *want[3] = { w, h, &maxval };
    for (int i = 0; i < 3; i++) {
        int c;
        while ((c = fgetc(f)) == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#')
            if (c == '#') while ((c = fgetc(f)) != '\n' && c != EOF) ;
        ungetc(c, f);
        if (fscanf(f, "%d", want[i]) != 1) goto bad;
    }
    fgetc(f);                                       /* the one whitespace */
    if (maxval != 255 || *w <= 0 || *h <= 0) goto bad;
    size_t n = (size_t)*w * (size_t)*h * 3;
    unsigned char *rgb = malloc(n);
    if (!rgb || fread(rgb, 1, n, f) != n) { free(rgb); goto bad; }
    fclose(f);
    return rgb;
bad:
    fprintf(stderr, "guidepics: %s is not a binary PPM with maxval 255\n", path);
    fclose(f);
    return NULL;
}

/* Write one picture's palette, data and table entry text; returns 0 on
 * failure (too many colors). */
static int emit(const char *name, const unsigned char *rgb, int w, int h, FILE *table)
{
    unsigned long pal[256];
    int np = 0;
    size_t npx = (size_t)w * (size_t)h;
    unsigned char *idx = malloc(npx);
    if (!idx) return 0;
    for (size_t i = 0; i < npx; i++) {
        unsigned long c = (unsigned long)rgb[3 * i] << 16 | (unsigned long)rgb[3 * i + 1] << 8 | rgb[3 * i + 2];
        int k;
        for (k = 0; k < np && pal[k] != c; k++) ;
        if (k == np) {
            if (np == 256) { fprintf(stderr, "guidepics: %s has more than 256 colors\n", name); free(idx); return 0; }
            pal[np++] = c;
        }
        idx[i] = (unsigned char)k;
    }
    printf("static const unsigned long pal_%s[%d] = {", name, np);
    for (int k = 0; k < np; k++) printf("%s0x%06lx", k == 0 ? "\n    " : k % 8 ? ", " : ",\n    ", pal[k]);
    printf("\n};\nstatic const unsigned char rle_%s[] = {", name);
    size_t bytes = 0;
    for (size_t i = 0; i < npx; ) {
        size_t j = i + 1;
        while (j < npx && idx[j] == idx[i]) j++;
        unsigned long len = (unsigned long)(j - i - 1);
        printf("%s%u", bytes == 0 ? "\n    " : bytes % 20 ? "," : ",\n    ", idx[i]);
        bytes++;
        do {
            unsigned b = len & 0x7f;
            len >>= 7;
            printf("%s%u", bytes % 20 ? "," : ",\n    ", b | (len ? 0x80u : 0));
            bytes++;
        } while (len);
        i = j;
    }
    printf("\n};\n\n");
    fprintf(table, "    { \"%s\", %d, %d, %d, pal_%s, rle_%s, sizeof rle_%s },\n",
            name, w, h, np, name, name, name);
    fprintf(stderr, "guidepics: %-18s %4dx%-4d %3d colors, %6zu bytes\n", name, w, h, np, bytes);
    free(idx);
    return 1;
}

/* See the comment at the top. */
int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: guidepics NAME=FILE.ppm ...\n"); return 2; }
    FILE *table = tmpfile();
    if (!table) { perror("tmpfile"); return 1; }
    printf("/* guidepics.c - the screenshots of Help > How to Play.\n"
           " * Made by docs/guidepics from docs/guide/<name>.png (make guide-pics);\n"
           " * do not edit. The pictures are screenshots of XLandlord itself,\n"
           " * under the same license as the program. */\n\n"
           "#include <stddef.h>\n#include \"guidepics.h\"\n\n");
    for (int a = 1; a < argc; a++) {
        char name[64];
        const char *eq = strchr(argv[a], '=');
        if (!eq || eq == argv[a] || (size_t)(eq - argv[a]) >= sizeof name) {
            fprintf(stderr, "guidepics: expected NAME=FILE, got %s\n", argv[a]);
            return 2;
        }
        memcpy(name, argv[a], (size_t)(eq - argv[a]));
        name[eq - argv[a]] = '\0';
        for (char *p = name; *p; p++) if (*p == '-') *p = '_';     /* a C name */
        int w, h;
        unsigned char *rgb = read_ppm(eq + 1, &w, &h);
        if (!rgb || !emit(name, rgb, w, h, table)) return 1;
        free(rgb);
    }
    printf("const GuidePic guide_pics[] = {\n");
    rewind(table);
    int c;
    while ((c = fgetc(table)) != EOF) putchar(c);
    printf("    { NULL, 0, 0, 0, NULL, NULL, 0 }\n};\n");
    return 0;
}
