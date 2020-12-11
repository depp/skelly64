#include "base/tool.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char FORMATS[4][5] = {
    "RGBA",
    "CI",
    "IA",
    "I",
};

static void check_fmt(const char *fmt) {
    for (int i = 0; i < 4; i++) {
        if (strcmp(FORMATS[i], fmt) == 0) {
            return;
        }
    }
    die("invalid format");
}

static int ilog2(int x) {
    for (int i = 0; i < 30; i++) {
        if (x == 1 << i) {
            return i;
        }
    }
    die("not a power of two: %d", x);
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fputs(
            "Usage: texture_dl_gen <out> <fmt> <size> <width> <height> "
            "<levels>\n"
            "                      [repeat]\n",
            stderr);
        die("bad usage");
    }
    FILE *out;
    if (strcmp(argv[1], "-") == 0) {
        out = stdout;
    } else {
        out = fopen(argv[1], "w");
        if (out == NULL) {
            die_errno(errno, "could not open output");
        }
    }
    const char *const fmt = argv[2];
    check_fmt(fmt);
    const int bitsize = xatoi(argv[3]);
    const int xsz = xatoi(argv[4]);
    const int ysz = xatoi(argv[5]);
    const int nlevel = xatoi(argv[6]);
    bool do_repeat = false;
    for (int i = 7; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "repeat") == 0) {
            do_repeat = true;
        }
    }
    int xmask = 0, ymask = 0;
    if (do_repeat) {
        xmask = ilog2(xsz);
        ymask = ilog2(ysz);
        if (xmask + 1 < nlevel || ymask + 1 < nlevel) {
            die("too many levels");
        }
    }

    // Calculate total size.
    int size = 0;
    int x = xsz, y = ysz;
    for (int level = 0; level < nlevel; level++) {
        int stride = (x * bitsize + 63) >> 6;
        size += stride * y;
        x >>= 1;
        y >>= 1;
    }

    // Load data into TMEM.
    fputs( //
        "gsDPSetTile("
        "G_IM_FMT_RGBA, " // format
        "G_IM_SIZ_16b, "  // pixel size
        "0, "             // size of one row
        "0, "             // tmem address
        "G_TX_LOADTILE, " // tile
        "0, "             // palette
        "G_TX_NOMIRROR, " // cmt
        "0, "             // maskt
        "G_TX_NOLOD, "    // shiftt
        "G_TX_NOMIRROR, " // cms
        "0, "             // masks
        "G_TX_NOLOD),\n", // shifts
        out);
    fputs("gsDPLoadSync(),\n", out);
    fprintf( //
        out,
        "gsDPLoadBlock("
        "G_TX_LOADTILE, " // tile
        "0, "             // uls
        "0, "             // ult
        "%d, "            // lrs
        "0),\n",          // dxt
        (size << 2) - 1);
    fputs("gsDPPipeSync(),\n", out);

    // Set up tiles.
    x = xsz;
    y = ysz;
    int pos = 0;
    for (int level = 0; level < nlevel; level++) {
        int stride = (x * bitsize + 63) >> 6;
        int masks = do_repeat ? xmask - level : 0;
        int maskt = do_repeat ? ymask - level : 0;
        fprintf( //
            out,
            "gsDPSetTile("
            "G_IM_FMT_%s, "  // format
            "G_IM_SIZ_%db, " // size
            "%d, "           // stride
            "%d, "           // tmem address
            "%d, "           // tile
            "0, "            // palette
            "0, "            // cmt
            "%d, "           // maskt
            "%d, "           // shiftt
            "0, "            // cms
            "%d, "           // masks
            "%d),\n",        // shifts
            fmt, bitsize, stride, pos, level, maskt, level, masks, level);
        fprintf( //
            out,
            "gsDPSetTileSize("
            "%d, "                            // tile
            "0, "                             // uls
            "0, "                             // ult
            "%d << G_TEXTURE_IMAGE_FRAC, "    // lrs
            "%d << G_TEXTURE_IMAGE_FRAC),\n", // ltr
            level, x - 1, y - 1);
        pos += stride * y;
        x >>= 1;
        y >>= 1;
    }

    if (out != stdout && fclose(out) != 0) {
        die_errno(errno, "could not write output");
    }
    return 0;
}
