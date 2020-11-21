// Font rasterization tool. Converts fonts to raster images. Automatically
// figures out the correct scale for bitmap fonts and pixel fonts, and this
// works correctly both for fonts with bitmap strikes (proper bitmap fonts) and
// for modern TTF bitmaps that have been converted to outlines.
#include "base/tool.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define FT_ERRORDEF(e, v, s) {v, s},
#define FT_ERROR_START_LIST {
#define FT_ERROR_END_LIST }

struct fterrorinfo {
    int code;
    const char *msg;
};

static const struct fterrorinfo FT_ERROR_INFOS[] =
#undef __FTERRORS_H__
#include FT_ERRORS_H
    ;

static void usage(FILE *fp) {
    fputs(
        "Usage:\n"
        "    raster dump <font>\n"
        "    raster pixel-size <font>\n"
        "    raster rasterize -font=<font> [-size=<size>|auto]\n"
        "                     [-output=<file>] [-mode=gray|mono]\n",
        fp);
}

// Print an error message with an error from FreeType and exit.
static noreturn void die_freetype(FT_Error err, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static noreturn void die_freetype(FT_Error err, const char *fmt, ...) {
    fputs("Error: ", stderr);
    va_list(ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs(": ", stderr);
    bool have_error = false;
    for (size_t i = 0; i < ARRAY_COUNT(FT_ERROR_INFOS); i++) {
        if (FT_ERROR_INFOS[i].code == err) {
            fputs(FT_ERROR_INFOS[i].msg, stderr);
            have_error = true;
        }
    }
    if (!have_error) {
        fprintf(stderr, "unknown FreeType error (err=%d)", err);
    }
    fputc('\n', stderr);
    exit(1);
}

enum {
    ERR_MULTIPLE_FIXED_SIZES = -1,
    ERR_NO_FIXED_OR_OUTLINES = -2,
    ERR_NOT_PIXEL_FONT = -3,
};

// Print an error for one of our custom error codes and exit.
static noreturn void die_cerr(int err, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static noreturn void die_cerr(int err, const char *fmt, ...) {
    fputs("Error: ", stderr);
    va_list(ap);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs(": ", stderr);
    const char *msg;
    switch (err) {
    case ERR_MULTIPLE_FIXED_SIZES:
        msg = "font has multiple fixed sizes";
        break;
    case ERR_NO_FIXED_OR_OUTLINES:
        msg = "font has no fixed sizes or outlines";
        break;
    case ERR_NOT_PIXEL_FONT:
        msg = "cannot autosize a font which is not a pixel font";
        break;
    default:
        msg = "unknown error";
        break;
    }
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

// A histogram of integer magnitudes.
struct histo {
    unsigned *ptr;
    int size;
};

// Add the magnitude of a coordinate to the histogram.
static void histo_add(struct histo *restrict histo, long value) {
    if (value <= INT_MIN || value > INT_MAX)
        die("coordinate too large: %ld", value);
    if (value < 0)
        value = -value;
    if (value >= histo->size) {
        int new_size = histo->size;
        if (new_size == 0)
            new_size = 64;
        do {
            if (new_size > INT_MAX / 2)
                die("font glyphs too large");
            new_size *= 2;
        } while (value >= new_size);
        unsigned *new_ptr = calloc(new_size, sizeof(*new_ptr));
        if (new_ptr == NULL)
            die_errno(errno, "could not allocate %zu bytes",
                      sizeof(*new_ptr) * new_size);
        memcpy(new_ptr, histo->ptr, sizeof(*histo->ptr) * histo->size);
        free(histo->ptr);
        histo->ptr = new_ptr;
        histo->size = new_size;
    }
    histo->ptr[value]++;
}

// Populate a histogram with coordinates of control points taken from all glyphs
// in the font.
static void histo_from_font(struct histo *restrict histo, FT_Face face) {
    FT_Error err;
    for (long i = 0, n = face->num_glyphs; i < n; i++) {
        err = FT_Load_Glyph(face, i, FT_LOAD_NO_SCALE);
        if (err != 0)
            die_freetype(err, "could not load glyph #%ld", i);
        FT_Outline *restrict ol = &face->glyph->outline;
        int npoints = ol->n_points;
        FT_Vector *restrict points = ol->points;
        for (int i = 0; i < npoints; i++) {
            histo_add(histo, points[i].x);
            histo_add(histo, points[i].y);
        }
    }
}

// Scale the coordinates in the histogram by the given amount and return the
// total distance of all coordinates from the closest integer coordinates.
static double histo_evaluate_scale(struct histo *restrict histo, double scale) {
    double sum = 0.0;
    unsigned *restrict ptr = histo->ptr;
    for (int i = 0; i < histo->size; i++) {
        if (ptr[i] != 0) {
            double fcoord = scale * (double)i;
            double offset = fcoord - rint(fcoord);
            sum += (double)ptr[i] * offset * offset;
        }
    }
    return sum * scale;
}

// Return the average of the squared distance that coordinates must move to the
// nearest integer coordinates.
static double histo_average_cost(struct histo *restrict histo, double scale) {
    unsigned *restrict ptr = histo->ptr;
    unsigned total = 0;
    for (int i = 0; i < histo->size; i++)
        total += ptr[i];
    if (total == 0)
        return 0.0;
    double sum = histo_evaluate_scale(histo, scale);
    return sum / (double)total;
}

// Calculate the scale for a font to get most control points close to integer
// coordinates.
static int histo_pixel_size(struct histo *restrict histo, long em_size) {
    // From the FreeType documentation for FT_Set_Char_Size:
    //
    // > While this function allows fractional points as input values, the
    // > resulting ppem value for the given resolution is always rounded to the
    // > nearest integer.
    //
    // So we only evaluate different ways to divide up the em.
    double invx = 1.0 / em_size;
    int best_scale = 0;
    double best_cost = HUGE_VAL;
    for (int i = 4; i <= 64; i++) {
        double scale = (double)i * invx;
        // Multiply by scale to weight more heavily towards smaller scales.
        double cost = histo_evaluate_scale(histo, scale) * scale;
        if (cost < best_cost) {
            best_scale = i;
            best_cost = cost;
        }
    }
    return best_scale;
}

// "Extreme" values for autodetecting sizes, just as a sanity check.
enum {
    MIN_AUTO_SIZE = 4,
    MAX_AUTO_SIZE = 128,
};

// Get the automatic size for a pixel font.
static int font_autosize(FT_Face face) {
    // Look for fixed sizes (raster font).
    if (face->num_fixed_sizes > 0) {
        if (face->num_fixed_sizes != 1)
            return ERR_MULTIPLE_FIXED_SIZES;
        long ppem = face->available_sizes[0].y_ppem;
        if (ppem < MIN_AUTO_SIZE * 64)
            die("fixed font size is too small: %f", (double)ppem / 64.0);
        if (ppem > MAX_AUTO_SIZE * 64)
            die("fixed font size is too large: %f", (double)ppem / 64.0);
        return (ppem + 32) >> 6;
    }
    struct histo histo = {};
    histo_from_font(&histo, face);
    if (histo.size == 0)
        return ERR_NO_FIXED_OR_OUTLINES;
    int size = histo_pixel_size(&histo, face->units_per_EM);
    // printf("Empirical pixel size: %d\n", size);
    // double scale = (double)size / (double)face->units_per_EM;
    // double avg_cost = histo_evaluate_scale(&histo, scale) /
    // (double)histo.total; printf("")
    free(histo.ptr);
    return size;
}

// Command to dump font information.
static int cmd_dump(int argc, char **argv) {
    if (argc != 1)
        die("dump: got %d arguments, expected exactly 1", argc);
    const char *arg_font = argv[0];
    FT_Library lib;
    FT_Error err;
    err = FT_Init_FreeType(&lib);
    if (err != 0)
        die_freetype(err, "could not init FreeType");
    FT_Face face;
    err = FT_New_Face(lib, arg_font, 0, &face);
    if (err != 0)
        die_freetype(err, "could not open font %s", arg_font);
    printf("Num glyphs: %ld\n", face->num_glyphs);
    FT_Done_FreeType(lib);
    return 0;
}

// Command to figure out the pixel size for a pixel font.
static int cmd_pixel_size(int argc, char **argv) {
    if (argc != 1)
        die("pixel-size: got %d arguments, expected exactly 1", argc);
    const char *arg_font = argv[0];
    FT_Library lib;
    FT_Error err;
    err = FT_Init_FreeType(&lib);
    if (err != 0)
        die_freetype(err, "could not init FreeType");
    FT_Face face;
    err = FT_New_Face(lib, arg_font, 0, &face);
    if (err != 0)
        die_freetype(err, "could not open font %s", arg_font);
    bool have_size = false;
    if (face->num_fixed_sizes > 0) {
        for (int i = 0; i < face->num_fixed_sizes; i++) {
            have_size = true;
            long size = face->available_sizes[0].y_ppem;
            printf("Fixed size: %ld\n", (size + 32) >> 6);
        }
    }
    struct histo histo = {};
    histo_from_font(&histo, face);
    if (histo.size != 0) {
        have_size = true;
        int size = histo_pixel_size(&histo, face->units_per_EM);
        printf("Empirical pixel size: %d\n", size);
        double scale = (double)size / (double)face->units_per_EM;
        double cost = histo_average_cost(&histo, scale);
        printf("Cost: %f\n", cost);
    }
    if (!have_size)
        die("could not get pixel size: no fixed sizes, no outlines");
    FT_Done_FreeType(lib);
    return 0;
}

enum {
    MODE_GRAY,
    MODE_MONO,
};

enum {
    SIZE_AUTO = -1,
};

static const char *const ERR_OUTPUT = "could not write output";

static void xprintf(FILE *out, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
static void xprintf(FILE *out, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(out, fmt, ap);
    if (r < 0)
        die_errno(errno, ERR_OUTPUT);
    va_end(ap);
}

static void xputc(FILE *out, int c) {
    int r = putc(c, out);
    if (r == EOF)
        die_errno(errno, ERR_OUTPUT);
}

static void xputs(FILE *out, const char *s) {
    int r = fputs(s, out);
    if (r == EOF)
        die_errno(errno, ERR_OUTPUT);
}

static int cmd_rasterize(int argc, char **argv) {
    int arg_mode = MODE_GRAY;
    int arg_size = 0;
    const char *arg_font = NULL;
    const char *arg_output = NULL;
    for (int i = 0; i < argc; i++) {
        char *arg = argv[i];
        if (arg[0] != '-') {
            die("rasterize: unexpected argument: '%s'", arg);
        }
        char *opt = arg + 1;
        if (*opt == '-')
            opt++;
        char *eq = strchr(opt, '=');
        char *value = NULL;
        if (eq != NULL) {
            *eq = '\0';
            value = eq + 1;
        }
        if (strcmp(opt, "mode") == 0) {
            if (value == NULL)
                die("-mode requires parameter -mode=<mode>");
            if (strcmp(value, "gray") == 0 || strcmp(value, "grey") == 0) {
                arg_mode = MODE_GRAY;
            } else if (strcmp(value, "mono") == 0) {
                arg_mode = MODE_MONO;
            } else {
                die("unknown value for -mode: '%s'", value);
            }
        } else if (strcmp(opt, "size") == 0) {
            if (value == NULL)
                die("-size requires parameter -size=<size>");
            if (strcmp(value, "auto") == 0) {
                arg_size = SIZE_AUTO;
            } else {
                errno = 0;
                char *end;
                unsigned long x = strtoul(value, &end, 10);
                if (*value == '\0' || *end != '\0' || errno != 0)
                    die("invalid size: '%s'", value);
                if (x == 0)
                    die("invalid size: 0");
                if (x > 32767)
                    die("size too large: %ld", x);
                arg_size = x;
            }
        } else if (strcmp(opt, "font") == 0) {
            if (value == NULL)
                die("-font requires parameter -font=<font>");
            arg_font = value;
        } else if (strcmp(opt, "output") == 0) {
            if (value == NULL)
                die("-output requires parameter -output=<file>");
            arg_output = value;
        } else {
            if (eq != NULL) {
                *eq = '=';
            }
            die("rasterize: unknown flag: '%s'", arg);
        }
    }
    if (arg_font == NULL)
        die("missing required flag -font");
    FT_Library lib;
    FT_Error err;
    err = FT_Init_FreeType(&lib);
    if (err != 0)
        die_freetype(err, "could not init FreeType");
    FT_Face face;
    err = FT_New_Face(lib, arg_font, 0, &face);
    if (err != 0)
        die_freetype(err, "could not open font %s", arg_font);
    int size;
    if (arg_size > 0) {
        size = arg_size;
    } else {
        size = font_autosize(face);
        if (size < 0) {
            if (arg_size == SIZE_AUTO)
                die_cerr(size, "could not automatically size font");
            size = 16;
            fprintf(stderr, "Default font size: %d\n", size);
        } else {
            fprintf(stderr, "Automatic font size: %d\n", size);
        }
    }
    err = FT_Set_Char_Size(face, size << 6, size << 6, 72, 72);
    if (err != 0)
        die_freetype(err, "could not set font size");
    int load_flags, render_flags;
    switch (arg_mode) {
    case MODE_GRAY:
        load_flags = FT_LOAD_TARGET_NORMAL;
        render_flags = FT_RENDER_MODE_NORMAL;
        break;
    case MODE_MONO:
        load_flags = FT_LOAD_TARGET_MONO;
        render_flags = FT_RENDER_MODE_MONO;
        break;
    default:
        die("invaild mode");
    }
    bool have_charmap = false;
    for (long i = 0, n = face->num_charmaps; i < n; i++) {
        FT_CharMap cmap = face->charmaps[i];
        if (cmap->encoding == FT_ENCODING_UNICODE) {
            err = FT_Set_Charmap(face, cmap);
            if (err != 0)
                die_freetype(err, "could not set charmap");
            have_charmap = true;
            break;
        }
    }
    if (!have_charmap)
        die("font has no Unicode character map");
    FILE *out = stdout;
    if (arg_output != NULL) {
        out = fopen(arg_output, "w");
        if (out == NULL)
            die_errno(errno, "could not open output '%s'", arg_output);
    }
    // Dump font metrics.
    {
        FT_Size_Metrics *m = &face->size->metrics;
        xprintf(out, "metrics %ld %ld %ld\n", (m->ascender + 32) >> 6,
                (m->descender + 2) >> 6, (m->height + 32) >> 6);
    }
    // Dump character map.
    {
        unsigned int gindex = 0;
        unsigned long charcode = FT_Get_First_Char(face, &gindex);
        while (gindex != 0) {
            xprintf(out, "char %lu %u\n", charcode, gindex);
            charcode = FT_Get_Next_Char(face, charcode, &gindex);
        }
    }
    // Dump glyphs.
    for (long i = 0, n = face->num_glyphs; i < n; i++) {
        char glyphname[128];
        if (FT_HAS_GLYPH_NAMES(face)) {
            err = FT_Get_Glyph_Name(face, i, glyphname, sizeof(glyphname));
            if (err != 0)
                die_freetype(err, "could not get name for glyph %ld", i);
            for (size_t i = 0; glyphname[i] != '\0'; i++) {
                if (glyphname[i] <= ' ' || glyphname[i] >= 127) {
                    die_freetype(err, "invalid name for glyph %ld", i);
                }
            }
        } else {
            glyphname[0] = '\0';
        }
        err = FT_Load_Glyph(face, i, load_flags);
        if (err != 0)
            die_freetype(err, "could not load glyph %ld", i);
        FT_GlyphSlot glyph = face->glyph;
        err = FT_Render_Glyph(glyph, render_flags);
        if (err != 0)
            die_freetype(err, "could not render glyph %ld", i);
        int width = glyph->bitmap.width;
        int height = glyph->bitmap.rows;
        int pitch = glyph->bitmap.pitch;
        int advance = (glyph->advance.x + 32) >> 6;
        xprintf(out, "glyph %u %u %d %d %d ", width, height, glyph->bitmap_left,
                glyph->bitmap_top, advance);
        if (glyphname[0] != '\0') {
            xputs(out, glyphname);
        } else {
            xputc(out, '-');
        }
        xputc(out, ' ');
        if (width > 0 && height > 0) {
            const unsigned char *buf = glyph->bitmap.buffer;
            switch (glyph->bitmap.pixel_mode) {
            case FT_PIXEL_MODE_GRAY:
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        xprintf(out, "%02X", buf[y * pitch + x]);
                    }
                }
                break;
            case FT_PIXEL_MODE_MONO:
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        unsigned d = buf[y * pitch + x / 8];
                        bool bit = (d & (0x80 >> (x & 7))) != 0;
                        xputs(out, bit ? "FF" : "00");
                    }
                }
                break;
            default:
                die("unsupported pixel mode: %d", glyph->bitmap.pixel_mode);
            }
        } else {
            xputc(out, '-');
        }
        xputc(out, '\n');
    }
    if (arg_output != NULL) {
        int r = fclose(out);
        if (r != 0)
            die_errno(errno, ERR_OUTPUT);
    }
    FT_Done_FreeType(lib);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(stdout);
        return 0;
    }
    const char *cmd = argv[1];
    argc -= 2;
    argv += 2;
    if (strcmp(cmd, "dump") == 0)
        return cmd_dump(argc, argv);
    if (strcmp(cmd, "pixel-size") == 0)
        return cmd_pixel_size(argc, argv);
    if (strcmp(cmd, "rasterize") == 0)
        return cmd_rasterize(argc, argv);
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "-h") == 0 ||
        strcmp(cmd, "--help") == 0 || strcmp(cmd, "-help") == 0) {
        usage(stdout);
        return 0;
    }
    fputs("Error: unknown command\n", stderr);
    return 64;
}
